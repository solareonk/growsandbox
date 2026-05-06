#pragma once
#include "TileRegistry.h"

class Selection
{
public:
    enum Kind { FIST, BLOCK };

    Selection();

    void SetFist();
    void SetBlock(TileTypeID type);
    Kind GetKind() const;
    TileTypeID GetBlockType() const;

private:
    Kind m_kind;
    TileTypeID m_blockType;
};
