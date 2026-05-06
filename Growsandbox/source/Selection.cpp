#include "PlatformPrecomp.h"
#include "Selection.h"

Selection::Selection() : m_kind(FIST), m_blockType(TILE_AIR) {}

void Selection::SetFist() { m_kind = FIST; m_blockType = TILE_AIR; }
void Selection::SetBlock(TileTypeID type) { m_kind = BLOCK; m_blockType = type; }
Selection::Kind Selection::GetKind() const { return m_kind; }
TileTypeID Selection::GetBlockType() const { return m_blockType; }
