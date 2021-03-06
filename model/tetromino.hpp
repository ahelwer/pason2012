#pragma once

#include <string>
#include <iostream>
#include <util/vector.hpp>

/* *
 * Class Tetromino
 *
 * Represents the Tetromino type, position, and orientation.
 * */
class Tetromino {
public:
    Tetromino(char tetType, int rotation = 0, 
                int x = 5, int y = 1);
    // Gets boolean array description of Tetromino
    bool const* GetDesc() const;
    char GetType() const;
    int GetOrient() const;
    int GetX() const;
    int GetY() const;
    void SetX(int x);
    void SetY(int y);
    void SetOrient(int orient);
    void ShiftRight();
    void ShiftLeft();
    void ShiftDown();
    void ShiftUp();
    void RotateLeft();
    void RotateRight();
    friend std::ostream& operator<< (std::ostream& out, Tetromino const& t);
    bool operator== (Tetromino const& o) const;
    bool operator!= (Tetromino const& o) const;
    enum Move {
        left,
        right,
        down,
        lrotate,
        rrotate,
        drop,
        nmoves
    };
private:
    bool const* GetDesc(char type, int orient) const;
    char m_type;
    int m_orient;
    int m_col;
    int m_row;
    bool const* m_pDesc;
};

typedef std::vector<enum Tetromino::Move> PathSequence;

