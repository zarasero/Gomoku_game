#include "Board.hpp"

namespace gomoku
{
    Board::Board()
    {
        clear();
    }

    void Board::clear()
    {
        for (int r = 0; r < BOARD_SIZE; ++r)
        {
            for (int c = 0; c < BOARD_SIZE; ++c)
            {
                cells_[r][c] = CellState::Empty;
            }
        }
    }

    bool Board::inBounds(int row, int col) const
    {
        return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
    }

    bool Board::isEmpty(int row, int col) const
    {
        return inBounds(row, col) && cells_[row][col] == CellState::Empty;
    }

    CellState Board::at(int row, int col) const
    {
        return cells_[row][col];
    }

    void Board::set(int row, int col, CellState value)
    {
        cells_[row][col] = value;
    }

    bool Board::hasAnyStone() const
    {
        for (int r = 0; r < BOARD_SIZE; ++r)
        {
            for (int c = 0; c < BOARD_SIZE; ++c)
            {
                if (cells_[r][c] != CellState::Empty)
                    return true;
            }
        }
        return false;
    }

    CellState opponentOf(CellState player)
    {
        return (player == CellState::Black) ? CellState::White : CellState::Black;
    }
}