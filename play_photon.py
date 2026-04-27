import chess
import chess.engine
import sys

def print_board(board):
    print("\n   a b c d e f g h")
    print("  +-----------------+")
    ranks = str(board).split('\n')
    for i, rank in enumerate(ranks):
        print(f"{8-i} | {' '.join(rank.split())} | {8-i}")
    print("  +-----------------+")
    print("   a b c d e f g h\n")

def main():
    print("===================================")
    print("   Play against Photon Engine!     ")
    print("===================================")
    
    # Initialize the engine
    try:
        engine = chess.engine.SimpleEngine.popen_uci("build/photon.exe")
    except FileNotFoundError:
        print("Error: build/photon.exe not found. Make sure it is compiled.")
        sys.exit(1)

    board = chess.Board()

    # Choose color
    user_color = input("Do you want to play as White or Black? (w/b): ").strip().lower()
    user_is_white = user_color == 'w'

    while not board.is_game_over():
        print_board(board)
        
        if (board.turn == chess.WHITE and user_is_white) or (board.turn == chess.BLACK and not user_is_white):
            # User's turn
            move = None
            while move not in board.legal_moves:
                move_str = input("Enter your move (e.g. e2e4): ").strip()
                try:
                    move = chess.Move.from_uci(move_str)
                    if move not in board.legal_moves:
                        print("Illegal move! Try again.")
                except ValueError:
                    print("Invalid format! Use UCI format (e.g. e2e4).")
            
            board.push(move)
        else:
            # Engine's turn
            print("Photon is thinking...")
            result = engine.play(board, chess.engine.Limit(time=1.0)) # 1 second per move
            print(f"Photon plays: {result.move.uci()}")
            board.push(result.move)

    print_board(board)
    print("Game Over!")
    print("Result:", board.result())
    engine.quit()

if __name__ == "__main__":
    main()
