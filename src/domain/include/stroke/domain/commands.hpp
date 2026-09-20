#pragma once

#include "stroke/domain/types.hpp"

#include <cstddef>
#include <variant>

namespace stroke {

struct AppendStroke { Stroke value; };
struct Backspace {};
struct Cancel {};
enum class PageDirection { previous, next };
struct ChangePage { PageDirection direction; };
// Zero-based index into a visible snapshot. The engine rejects stale revisions.
struct SelectCandidate { std::uint64_t revision; std::size_t index; };
using Command = std::variant<AppendStroke, Backspace, Cancel, ChangePage, SelectCandidate>;

} // namespace stroke
