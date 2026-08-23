#pragma once

#include <optional>
#include <string_view>

#include "kfc/realtime/motion_kind.hpp"
#include "kfc/util/enum_names.hpp"

namespace kfc::model {

/// See kfc/model/piece_names.hpp for why this lives next to the enum.
inline constexpr kfc::util::EnumNames<MotionKind, 2> kMotionKindNames{{{
    {MotionKind::Move, "Move"},
    {MotionKind::JumpInPlace, "JumpInPlace"},
}}};

[[nodiscard]] constexpr std::string_view name_of(MotionKind kind) {
    return kMotionKindNames.name_of(kind);
}

[[nodiscard]] constexpr std::optional<MotionKind> motion_kind_from_name(std::string_view name) {
    return kMotionKindNames.value_of(name);
}

static_assert(kMotionKindNames.covers_through(MotionKind::JumpInPlace), "every MotionKind needs a name");

}  // namespace kfc::model
