#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace kfc::graphics {

/// The course-provided image wrapper (OpenCV-backed), vendored verbatim into
/// kfc's own namespace/layout -- same read/draw_on/put_text/show contract the
/// instructor specified, not a reimplementation. Everything downstream in
/// kfc::graphics builds on this instead of touching cv::Mat directly.
class Img {
public:
    Img();

    /// Loads the image at path (with alpha channel, if present) and
    /// optionally resizes it. If size is {0, 0} (the default), the image
    /// keeps its native pixel dimensions. keep_aspect shrinks so the longer
    /// side fits size while preserving aspect ratio, instead of stretching
    /// to size exactly. Throws std::runtime_error if path cannot be read.
    Img& read(const std::string& path, const std::pair<int, int>& size = {}, bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);

    /// Draws this image onto other_img with its top-left corner at (x, y),
    /// alpha-blending if this image has a fourth (alpha) channel. Throws
    /// std::runtime_error if either image is unloaded or this image would
    /// not fit inside other_img at that position.
    void draw_on(Img& other_img, int x, int y);

    /// Draws txt with its baseline's left edge at (x, y).
    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255), int thickness = 1);

    /// Opens a window showing the image and blocks until any key is pressed.
    void show();

    /// The underlying OpenCV matrix, for callers that need raw pixel access.
    const cv::Mat& get_mat() const {
        return img_;
    }

    /// A new Img holding an independent deep copy of this image's pixels.
    /// Img's own (compiler-generated) copy constructor only copies cv::Mat's
    /// reference-counted handle -- the copy would still share the same pixel
    /// buffer as the original, so drawing on one would silently corrupt the
    /// other. Needed for a per-frame render loop: clone a loaded background
    /// once, then draw fresh pieces onto a new clone every frame, without
    /// re-reading the file from disk each time.
    Img clone() const;

    /// A new Img of the given size, filled with color -- the only way to
    /// get pixel data into an Img without reading a file. Needed to compose
    /// a canvas bigger than any single loaded asset (e.g. a board texture
    /// plus an empty side panel with nothing on disk to load for it).
    static Img blank(int width, int height, const cv::Scalar& color = cv::Scalar(0, 0, 0, 255));

    /// A new Img holding an independent deep copy of the width x height
    /// region starting at (x, y) -- e.g. pulling just the playable grid out
    /// of a board texture that also has a decorative frame baked in around
    /// it. Throws std::runtime_error if that region doesn't fit inside this
    /// image, the same way draw_on does for the opposite case.
    Img cropped(int x, int y, int width, int height) const;

    /// A new Img holding an independent deep copy of this image, resized to
    /// width x height -- e.g. rescaling the whole background/board/HUD
    /// composition to fit however large the app's window currently is.
    Img resized(int width, int height) const;

    /// "cover"-scales and center-crops this image to exactly target_width x
    /// target_height: scales up just enough that both dimensions meet or
    /// exceed the target (preserving aspect ratio, so one dimension
    /// typically overshoots), then crops the centered target_width x
    /// target_height region out of that -- the same fit CSS's
    /// background-size: cover gives, showing as much of the source as the
    /// target's aspect ratio allows rather than stretching or letterboxing
    /// it. Used to fit a background image over an arbitrary window size
    /// without distorting it.
    Img cover_scaled(int target_width, int target_height) const;

    /// Drops this image's alpha channel (if it has one) and marks it fully
    /// opaque, regardless of whatever residual near-255 alpha noise its
    /// source PNG happens to carry (e.g. anti-aliased edges) -- for a layer
    /// meant to always paste as a hard, un-blended backdrop. Without this,
    /// draw_on's expensive per-pixel double-precision blend can trigger on
    /// a large image every frame for no visual benefit, the same
    /// board-texture-alpha pitfall this class already guards against for a
    /// single flag flip, just not automatically for a freshly cropped or
    /// otherwise-derived image.
    void force_opaque();

    /// Draws a translucent overlay across the bottom of a cell_size x
    /// cell_size cell whose top-left is at (cell_x, cell_y), covering
    /// fraction (0..1) of the cell's height. The overlay stays pinned to the
    /// cell's bottom edge -- as fraction counts down from 1.0 (cell fully
    /// covered, rest just started) to 0.0 (cell fully clear, rest over), its
    /// top edge drops, revealing the piece underneath from the top down. A
    /// "no art asset needed" stand-in for an hourglass's remaining sand,
    /// still piled against the neck at the bottom.
    void draw_hourglass_overlay(int cell_x, int cell_y, int cell_size, double fraction, const cv::Scalar& color);

    /// False until read() has successfully loaded an image.
    bool is_loaded() const {
        return !img_.empty();
    }

private:
    cv::Mat img_;
    // True only if img_ has a 4th channel whose values actually vary below
    // 255 somewhere -- computed once, whenever pixel data is (re)loaded, not
    // on every draw_on call. Lets draw_on skip its expensive per-pixel blend
    // for images that happen to have an alpha channel but are fully opaque
    // (e.g. a board texture exported as PNG-with-alpha, alpha=255
    // everywhere) -- see draw_on for why that distinction matters.
    bool has_transparency_ = false;
    void update_transparency_flag();
};

}  // namespace kfc::graphics
