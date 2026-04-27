import http.server
import socketserver
import json
import urllib.parse
import chess
import chess.engine
import chess.pgn
import io
import os
import sys

# Change to the directory of this script
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Global state
board = chess.Board()
try:
    engine = chess.engine.SimpleEngine.popen_uci("build/photon.exe")
except FileNotFoundError:
    print("Error: build/photon.exe not found.")
    sys.exit(1)

class ChessHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            with open("index.html", "rb") as f:
                self.wfile.write(f.read())
        elif self.path == '/newgame':
            board.reset()
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"fen": board.fen(), "pgn": ""}).encode())
        else:
            super().do_GET()

    def do_POST(self):
        if self.path == '/play':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            req = json.loads(post_data.decode('utf-8'))
            
            if "fen" in req:
                try:
                    board.set_fen(req["fen"])
                except Exception:
                    pass

            user_move = req.get("move")
            if user_move:
                try:
                    move = chess.Move.from_uci(user_move)
                    if move in board.legal_moves:
                        board.push(move)
                    else:
                        print(f"Illegal move received: {user_move} for fen: {board.fen()}")
                        self.send_error(400, "Illegal move")
                        return
                except:
                    self.send_error(400, "Invalid move format")
                    return

            if req.get("mode") != "friend" and not board.is_game_over():
                think_time = float(req.get("time", 0.5))
                result = engine.play(board, chess.engine.Limit(time=think_time))
                if result.move:
                    board.push(result.move)

            pgn_str = str(chess.pgn.Game.from_board(board))
            
            response = {
                "fen": board.fen(),
                "pgn": pgn_str,
                "game_over": board.is_game_over(),
                "result": board.result() if board.is_game_over() else "*"
            }
            
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())

        elif self.path == '/analyze':
            content_length = int(self.headers.get('Content-Length', 0))
            if content_length > 0:
                post_data = self.rfile.read(content_length)
                try:
                    req = json.loads(post_data.decode('utf-8'))
                    if "fen" in req:
                        board.set_fen(req["fen"])
                except Exception:
                    pass

            # Run a dynamic analysis with MultiPV=3 (if supported)
            multipv = 3
            try:
                results = engine.analyse(board, chess.engine.Limit(time=0.3), multipv=multipv)
            except chess.engine.EngineError:
                # Fallback for engines that don't support MultiPV (like older Photon builds)
                results = [engine.analyse(board, chess.engine.Limit(time=0.3))]
            
            # If engine returns single result (not list), wrap it
            if not isinstance(results, list):
                results = [results]

            lines = []
            for info in results:
                score_str = "0.00"
                if "score" in info:
                    sc = info["score"].white()
                    if sc.is_mate(): score_str = f"M{sc.mate()}"
                    else: score_str = f"{(sc.score() / 100.0):.2f}"
                
                pv_moves = info.get("pv", [])
                formatted_pv = []
                tmp_board = board.copy()
                
                move_num = tmp_board.fullmove_number
                is_white = tmp_board.turn == chess.WHITE
                
                for i, m in enumerate(pv_moves):
                    san = tmp_board.san(m)
                    uci = m.uci()
                    if i == 0:
                        prefix = f"{move_num}. " if is_white else f"{move_num}... "
                    else:
                        if is_white: prefix = f"{move_num}. "
                        else: prefix = ""
                    
                    formatted_pv.append({"san": san, "uci": uci, "prefix": prefix})
                    
                    if not is_white: move_num += 1
                    is_white = not is_white
                    tmp_board.push(m)

                lines.append({
                    "score": score_str,
                    "pv": formatted_pv,
                    "depth": info.get("depth", 0)
                })

            response = {
                "lines": lines
            }
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(response).encode())

class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True

PORT = 8000
with ReusableTCPServer(("", PORT), ChessHandler) as httpd:
    print(f"==================================================")
    print(f" Photon GUI is running! Open your browser to: ")
    print(f" http://localhost:{PORT}")
    print(f"==================================================")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    print("\nShutting down...")
    engine.quit()
