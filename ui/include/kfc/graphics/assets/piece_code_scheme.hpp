#pragma once

#include <string>

#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Maps a piece's (kind, color) to the folder name its asset pack uses for
/// it. Exists because pieces1 and pieces_mine encode this differently --
/// pieces1 is "KIND+COLOR" uppercase ("PW" = white pawn), pieces_mine is
/// "color+KIND" ("wP" = white pawn). One scheme per pack, so nothing else in
/// the graphics layer (PieceAssetLibrary, once it exists) ever hardcodes
/// either convention itself -- it just asks whichever scheme the active pack
/// was built with.
class IPieceCodeScheme {
public:
    virtual ~IPieceCodeScheme() = default;

    /// The folder name (directly under an asset pack's root directory) for
    /// this piece, e.g. "wP" or "PW" depending on the implementation.
    virtual std::string folder_name(kfc::model::PieceKind kind, kfc::model::PieceColor color) const = 0;
};

/// pieces_mine's scheme: lowercase color ('w'/'b') followed by an uppercase
/// kind letter (K Q R B N P). This is the pack kDefaultAssetPackName points
/// at.
class PiecesMineCodeScheme : public IPieceCodeScheme {
public:
    std::string folder_name(kfc::model::PieceKind kind, kfc::model::PieceColor color) const override;
};

}  // namespace kfc::graphics
