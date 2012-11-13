#pragma once

#include <util/vector.hpp>
#include <model/tetronimo.hpp>
#include <iostream>


typedef std::vector< std::vector<bool> > Board;

class GameBoard {
public:
    GameBoard();
    GameBoard(char const* desc);
    ~GameBoard();
	// Applies move to board, takes ownership of Tetronimo object
	bool PushMove(Tetronimo* t);
	// Reverts last move to board and deletes Tetronimo object
	bool PopMove();
	Board const& GetBoard() const;
	bool IsOccupied(int x, int y) const;
	bool InBounds(int x, int y) const;
	int WellDepth(int x) const;
	void Update(char const* desc);
    bool HasChanged(char const* desc) const;
	friend std::ostream& operator<< (std::ostream& out, GameBoard const& gb);
private:
	void Translate(char const* desc);
	Board m_board;
    char* m_pDesc;
	std::vector<Tetronimo*> m_moveStack;
};

