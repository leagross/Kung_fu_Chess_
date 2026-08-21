#include "../../../../include/kfc/graphics/primitives/img.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace kfc::graphics {

Img::Img() {}

Img Img::clone() const {
    Img copy;
    copy.img_ = img_.clone();
    copy.has_transparency_ = has_transparency_;
    return copy;
}

Img Img::blank(int width, int height, const cv::Scalar& color) {
    Img img;
    img.img_ = cv::Mat(height, width, CV_8UC4, color);
    img.has_transparency_ = color.val[3] < 255.0;
    return img;
}

void Img::update_transparency_flag() {
    if (img_.channels() != 4) {
        has_transparency_ = false;
        return;
    }
    std::vector<cv::Mat> channels;
    cv::split(img_, channels);
    double min_alpha = 255.0;
    double max_alpha = 0.0;
    cv::minMaxLoc(channels[3], &min_alpha, &max_alpha);
    has_transparency_ = min_alpha < 255.0;
}

void Img::draw_hourglass_overlay(int cell_x, int cell_y, int cell_size, double fraction, const cv::Scalar& color) {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    int overlay_height = static_cast<int>(cell_size * std::clamp(fraction, 0.0, 1.0));
    if (overlay_height <= 0) {
        return;
    }

    Img overlay = Img::blank(cell_size, overlay_height, color);
    overlay.draw_on(*this, cell_x, cell_y + (cell_size - overlay_height));
}

Img& Img::read(const std::string& path, const std::pair<int, int>& size, bool keep_aspect, int interpolation) {
    img_ = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img_.empty()) {
        throw std::runtime_error("Cannot load image: " + path);
    }

    if (size.first != 0 && size.second != 0) {
        int h = img_.rows;
        int w = img_.cols;

        if (keep_aspect) {
            double scale = std::min(static_cast<double>(size.first) / w, static_cast<double>(size.second) / h);
            cv::resize(img_, img_, cv::Size(static_cast<int>(w * scale), static_cast<int>(h * scale)), 0, 0,
                       interpolation);
        } else {
            cv::resize(img_, img_, cv::Size(size.first, size.second), 0, 0, interpolation);
        }
    }

    update_transparency_flag();
    return *this;
}

Img Img::cropped(int x, int y, int width, int height) const {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    if (x < 0 || y < 0 || x + width > img_.cols || y + height > img_.rows) {
        throw std::runtime_error("Crop region does not fit inside this image.");
    }

    Img result;
    result.img_ = img_(cv::Rect(x, y, width, height)).clone();
    result.update_transparency_flag();
    return result;
}

Img Img::resized(int width, int height) const {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    Img result;
    cv::resize(img_, result.img_, cv::Size(width, height));
    result.update_transparency_flag();
    return result;
}

Img Img::cover_scaled(int target_width, int target_height) const {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }

    double scale =
        std::max(static_cast<double>(target_width) / img_.cols, static_cast<double>(target_height) / img_.rows);
    // max() against the target guards against std::lround coming out a pixel
    // short after rounding, which would make cropped() reject the region.
    int scaled_width = std::max(target_width, static_cast<int>(std::lround(img_.cols * scale)));
    int scaled_height = std::max(target_height, static_cast<int>(std::lround(img_.rows * scale)));
    int crop_x = std::max(0, (scaled_width - target_width) / 2);
    int crop_y = std::max(0, (scaled_height - target_height) / 2);

    return resized(scaled_width, scaled_height).cropped(crop_x, crop_y, target_width, target_height);
}

void Img::force_opaque() {
    if (img_.channels() == 4) {
        cv::cvtColor(img_, img_, cv::COLOR_BGRA2BGR);
    }
    has_transparency_ = false;
}

void Img::draw_on(Img& other_img, int x, int y) {
    if (img_.empty() || other_img.img_.empty()) {
        throw std::runtime_error("Both images must be loaded before drawing.");
    }

    cv::Mat source_img = img_;
    cv::Mat& target_img = other_img.img_;

    if (source_img.channels() != target_img.channels()) {
        if (source_img.channels() == 3 && target_img.channels() == 4) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGR2BGRA);
        } else if (source_img.channels() == 4 && target_img.channels() == 3) {
            cv::cvtColor(source_img, source_img, cv::COLOR_BGRA2BGR);
        }
    }

    int h = source_img.rows;
    int w = source_img.cols;
    if (y + h > target_img.rows || x + w > target_img.cols) {
        throw std::runtime_error("Image does not fit at the specified position.");
    }

    cv::Mat roi = target_img(cv::Rect(x, y, w, h));

    if (source_img.channels() == 4 && has_transparency_) {
        // Per-pixel alpha blend. Must use Mat::mul (element-wise), not
        // operator* (matrix multiplication in OpenCV). CV_32F, not double:
        // 8-bit inputs need no more precision, and this runs every frame.
        std::vector<cv::Mat> source_channels;
        cv::split(source_img, source_channels);
        cv::Mat alpha;
        source_channels[3].convertTo(alpha, CV_32F, 1.0 / 255.0);

        std::vector<cv::Mat> roi_channels;
        cv::split(roi, roi_channels);

        for (int c = 0; c < 3; ++c) {
            cv::Mat source_channel_f;
            cv::Mat roi_channel_f;
            source_channels[c].convertTo(source_channel_f, CV_32F);
            roi_channels[c].convertTo(roi_channel_f, CV_32F);

            cv::Mat blended_f = alpha.mul(source_channel_f) + (1.0 - alpha).mul(roi_channel_f);
            blended_f.convertTo(roi_channels[c], roi.depth());
        }

        if (roi.channels() == 4) {
            // Standard "over" compositing for the destination's alpha too,
            // so an overlay painted onto a still-transparent region ends up
            // partially opaque rather than staying invisible.
            cv::Mat dst_alpha;
            roi_channels[3].convertTo(dst_alpha, CV_32F, 1.0 / 255.0);
            cv::Mat new_alpha_f = alpha + (1.0 - alpha).mul(dst_alpha);
            new_alpha_f.convertTo(roi_channels[3], roi.depth(), 255.0);
        }

        cv::merge(roi_channels, roi);
    } else {
        source_img.copyTo(roi);
    }
}

void Img::put_text(const std::string& txt, int x, int y, double font_size, const cv::Scalar& color, int thickness) {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    cv::putText(img_, txt, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX, font_size, color, thickness, cv::LINE_AA);
}

void Img::show() {
    if (img_.empty()) {
        throw std::runtime_error("Image not loaded.");
    }
    cv::imshow("Image", img_);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

}  // namespace kfc::graphics
