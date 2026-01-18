// list_navigator.h - Generic list navigation utilities
// Part of Mutables Daisies UI library
//
// Extracted from ui_state.h to deduplicate Next/Prev navigation patterns.
// Provides reusable navigation with wrapping and scroll management.

#pragma once

#include <cstdint>

namespace mutables_ui {
namespace nav {

// ============================================================================
// Basic Navigation (no scrolling)
// ============================================================================

// Move to next item with wrapping
// Returns new index
inline int Next(int current, int count) {
    if (count <= 0) return current;
    return (current + 1) % count;
}

// Move to previous item with wrapping
// Returns new index
inline int Prev(int current, int count) {
    if (count <= 0) return current;
    return (current - 1 + count) % count;
}

// ============================================================================
// Navigation with Title (-1 index represents title)
// ============================================================================

// Move next with title at -1 index
// current: -1 = title, 0+ = item index
// count: number of items (not including title)
// Returns: -1 for title, 0 to count-1 for items
inline int NextWithTitle(int current, int count) {
    if (count <= 0) return -1;  // No items, stay on title
    
    if (current == -1) {
        // From title, go to first item
        return 0;
    } else if (current >= count - 1) {
        // From last item, wrap to title
        return -1;
    } else {
        return current + 1;
    }
}

// Move previous with title at -1 index
inline int PrevWithTitle(int current, int count) {
    if (count <= 0) return -1;  // No items, stay on title
    
    if (current == -1) {
        // From title, go to last item
        return count - 1;
    } else if (current <= 0) {
        // From first item, wrap to title
        return -1;
    } else {
        return current - 1;
    }
}

// ============================================================================
// Scroll Management
// ============================================================================

// Update scroll offset to keep selected item visible
// Returns new scroll offset
inline int ScrollToSelected(int selected, int scroll_offset, int visible_count) {
    if (selected < scroll_offset) {
        return selected;
    } else if (selected >= scroll_offset + visible_count) {
        return selected - visible_count + 1;
    }
    return scroll_offset;
}

// Scroll to selected with bounds checking
inline int ScrollToSelectedBounded(int selected, int scroll_offset, 
                                   int visible_count, int total_count) {
    int new_offset = ScrollToSelected(selected, scroll_offset, visible_count);
    
    // Clamp to valid range
    if (new_offset < 0) new_offset = 0;
    int max_offset = total_count - visible_count;
    if (max_offset < 0) max_offset = 0;
    if (new_offset > max_offset) new_offset = max_offset;
    
    return new_offset;
}

// ============================================================================
// Combined Navigation + Scroll
// ============================================================================

// Navigation result containing new index and scroll offset
struct NavResult {
    int selected;
    int scroll_offset;
};

// Move next with automatic scroll update
inline NavResult NextWithScroll(int current, int scroll_offset, 
                                int count, int visible_count) {
    NavResult result;
    result.selected = Next(current, count);
    
    // Handle wrap-around: reset scroll to 0
    if (result.selected == 0 && current == count - 1) {
        result.scroll_offset = 0;
    } else {
        result.scroll_offset = ScrollToSelected(result.selected, scroll_offset, visible_count);
    }
    return result;
}

// Move previous with automatic scroll update
inline NavResult PrevWithScroll(int current, int scroll_offset, 
                                int count, int visible_count) {
    NavResult result;
    result.selected = Prev(current, count);
    
    // Handle wrap-around: scroll to show last item
    if (result.selected == count - 1 && current == 0) {
        result.scroll_offset = result.selected - visible_count + 1;
        if (result.scroll_offset < 0) result.scroll_offset = 0;
    } else {
        result.scroll_offset = ScrollToSelected(result.selected, scroll_offset, visible_count);
    }
    return result;
}

// Move next with title and scroll
inline NavResult NextWithTitleScroll(int current, int scroll_offset,
                                     int count, int visible_count) {
    NavResult result;
    result.selected = NextWithTitle(current, count);
    
    if (result.selected == -1) {
        // On title, scroll to top
        result.scroll_offset = 0;
    } else {
        result.scroll_offset = ScrollToSelected(result.selected, scroll_offset, visible_count);
    }
    return result;
}

// Move previous with title and scroll
inline NavResult PrevWithTitleScroll(int current, int scroll_offset,
                                     int count, int visible_count) {
    NavResult result;
    result.selected = PrevWithTitle(current, count);
    
    if (result.selected == -1) {
        // On title, scroll to top
        result.scroll_offset = 0;
    } else if (result.selected == count - 1 && current == -1) {
        // Wrapped from title to last item
        result.scroll_offset = result.selected - visible_count + 1;
        if (result.scroll_offset < 0) result.scroll_offset = 0;
    } else {
        result.scroll_offset = ScrollToSelected(result.selected, scroll_offset, visible_count);
    }
    return result;
}

// ============================================================================
// Visibility-Aware Navigation
// ============================================================================

// Find next visible item, using a visibility predicate
// Predicate signature: bool(int index)
template<typename VisibilityPredicate>
int NextVisible(int current, int count, VisibilityPredicate is_visible) {
    if (count <= 0) return current;
    
    int next = current;
    int attempts = 0;
    
    do {
        next = (next + 1) % count;
        attempts++;
    } while (!is_visible(next) && attempts < count);
    
    // If no visible items found, return original
    if (attempts >= count && !is_visible(next)) {
        return current;
    }
    return next;
}

// Find previous visible item
template<typename VisibilityPredicate>
int PrevVisible(int current, int count, VisibilityPredicate is_visible) {
    if (count <= 0) return current;
    
    int prev = current;
    int attempts = 0;
    
    do {
        prev = (prev - 1 + count) % count;
        attempts++;
    } while (!is_visible(prev) && attempts < count);
    
    // If no visible items found, return original
    if (attempts >= count && !is_visible(prev)) {
        return current;
    }
    return prev;
}

// Find first visible item starting from index
template<typename VisibilityPredicate>
int FindFirstVisible(int count, VisibilityPredicate is_visible, int start_from = 0) {
    for (int i = start_from; i < count; i++) {
        if (is_visible(i)) return i;
    }
    // Wrap around and search from beginning
    for (int i = 0; i < start_from; i++) {
        if (is_visible(i)) return i;
    }
    return -1;  // No visible items
}

// Find last visible item
template<typename VisibilityPredicate>
int FindLastVisible(int count, VisibilityPredicate is_visible) {
    for (int i = count - 1; i >= 0; i--) {
        if (is_visible(i)) return i;
    }
    return -1;  // No visible items
}

}  // namespace nav
}  // namespace mutables_ui
