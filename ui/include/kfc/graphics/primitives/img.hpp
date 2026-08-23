#pragma once

#include <opencv2/opencv.hpp>
#include <string>

namespace kfc::graphics {

/// OpenCV-backed image wrapper used throughout kfc::graphics instead of cv::Mat directly.
class Img {
public:
    Img();

    /// size {0,0} keeps native dimensions; keep_aspect shrinks to fit rather than stretching.
    Img& read(const std::string& path, const std::pair<int, int>& size = {}, bool keep_aspect = false,
              int interpolation = cv::INTER_AREA);

    /// Alpha-blends onto other_img at (x, y) if this image has an alpha channel.
    void draw_on(Img& other_img, int x, int y);

    void put_text(const std::string& txt, int x, int y, double font_size,
                  const cv::Scalar& color = cv::Scalar(255, 255, 255, 255), int thickness = 1);

    void show();

    const cv::Mat& get_mat() const {
        return img_;
    }

    /// Deep copy; cv::Mat's default copy would only share the reference-counted pixel buffer.
    Img clone() const;

    static Img blank(int width, int height, const cv::Scalar& color = cv::Scalar(0, 0, 0, 255));

    Img cropped(int x, int y, int width, int height) const;

    Img resized(int width, int height) const;

    /// CSS background-size:cover equivalent: scale to cover, then center-crop to target size.
    Img cover_scaled(int target_width, int target_height) const;

    /// Drops alpha and marks fully opaque, skipping draw_on's blend for near-255-alpha PNGs.
    void force_opaque();

    /// Bottom-fraction translucent overlay on a cell, standing in for an hourglass-sand effect.
    void draw_hourglass_overlay(int cell_x, int cell_y, int cell_size, double fraction, const cv::Scalar& color);

    bool is_loaded() const {
        return !img_.empty();
    }

private:
    cv::Mat img_;
    bool has_transparency_ = false;  // computed once on load so draw_on can skip its blend
    void update_transparency_flag();
};

}  // namespace kfc::graphics
