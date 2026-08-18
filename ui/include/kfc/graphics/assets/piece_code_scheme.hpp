#pragma once

#include <string>

#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Maps a piece's (kind, color) to the folder name its asset pack uses,
/// since different packs encode this differently (e.g. "PW" vs "wP").
class IPieceCodeScheme {
public:
    virtual ~IPieceCodeScheme() = default;

    /// Folder name (directly under an asset pack's root) for this piece.
    virtual std::string folder_name(kfc::model::PieceKind kind, kfc::model::PieceColor color) const = 0;
};

/// pieces_mine's scheme: lowercase color ('w'/'b') then uppercase kind
/// letter (K Q R B N P). The pack kDefaultAssetPackName points at.
class PiecesMineCodeScheme : public IPieceCodeScheme {
public:
    std::string folder_name(kfc::model::PieceKind kind, kfc::model::PieceColor color) const override;
};

}  // namespace kfc::graphics
