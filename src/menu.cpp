#include "menu.hpp"

#include <algorithm>

namespace de {

namespace {
const SDL_Color kBg{45, 45, 45, 235};
const SDL_Color kBorder{106, 161, 138, 255};
const SDL_Color kHover{70, 90, 80, 255};
const SDL_Color kText{235, 235, 235, 255};
const SDL_Color kTextDim{150, 150, 150, 255};
}  // namespace

void Menu::set_items(std::vector<MenuItem> items) { items_ = std::move(items); }

void Menu::set_checked(int id, bool checked) {
    for (MenuItem &item : items_) {
        if (item.id == id) {
            item.checked = checked;
        }
        for (MenuItem &sub : item.submenu) {
            if (sub.id == id) {
                sub.checked = checked;
            }
        }
    }
}

float Menu::item_height(const TextRenderer &text) const {
    int w = 0;
    int h = 0;
    if (text.measure("Ag", &w, &h) && h > 0) {
        return static_cast<float>(h) + 12.0f;
    }
    return row_h_;
}

float Menu::submenu_width(const std::vector<MenuItem> &items, const TextRenderer &text) const {
    float max_w = 0.0f;
    for (const MenuItem &item : items) {
        int w = 0;
        int h = 0;
        const std::string label = item.kind == MenuItem::Kind::Checkable
                                      ? (item.checked ? "[x] " : "[  ] ") + item.label
                                      : item.label;
        if (text.measure(label, &w, &h)) {
            max_w = std::max(max_w, static_cast<float>(w));
        }
    }
    return max_w + pad_ * 2.0f;
}

float Menu::menu_width(const TextRenderer &text) const { return submenu_width(items_, text); }

SDL_FRect Menu::item_rect(int index) const {
    return SDL_FRect{x_, y_ + static_cast<float>(index) * row_h_, width_, row_h_};
}

SDL_FRect Menu::submenu_item_rect(int parent_index, int child_index) const {
    (void)parent_index;
    return SDL_FRect{sub_x_, sub_y_ + static_cast<float>(child_index) * row_h_, sub_width_,
                     row_h_};
}

void Menu::update_submenu_geometry() {
    if (open_sub_ < 0 || open_sub_ >= static_cast<int>(items_.size())) {
        sub_x_ = 0.0f;
        sub_y_ = 0.0f;
        sub_h_ = 0.0f;
        return;
    }
    const std::vector<MenuItem> &sub = items_[open_sub_].submenu;
    sub_h_ = static_cast<float>(sub.size()) * row_h_;
    sub_x_ = x_ + width_;
    sub_y_ = y_ + static_cast<float>(open_sub_) * row_h_;

    if (sub_x_ + sub_width_ > screen_w_) {
        // Flip to the left of the parent panel; fall back to clamping.
        sub_x_ = x_ - sub_width_;
        if (sub_x_ < 0.0f) {
            sub_x_ = std::max(0.0f, screen_w_ - sub_width_);
        }
    }
    if (sub_y_ + sub_h_ > screen_h_) {
        sub_y_ = std::max(0.0f, screen_h_ - sub_h_);
    }
}

void Menu::refresh_layout(const TextRenderer &text) {
    row_h_ = item_height(text);
    width_ = menu_width(text);
    sub_width_ = width_;
}

void Menu::open(float x, float y, float screen_w, float screen_h) {
    x_ = x;
    y_ = y;
    visible_ = true;
    hover_ = -1;
    hover_sub_ = -1;
    open_sub_ = -1;

    const float total_h = static_cast<float>(items_.size()) * row_h_;
    const float total_w = width_ + sub_width_;
    if (x_ + total_w > screen_w) {
        x_ = std::max(0.0f, screen_w - total_w);
    }
    if (y_ + total_h > screen_h) {
        y_ = std::max(0.0f, screen_h - total_h);
    }
}

std::vector<SDL_Rect> Menu::interactive_rects() const {
    std::vector<SDL_Rect> rects;
    if (!visible_) {
        return rects;
    }
    const int total_h = static_cast<int>(static_cast<float>(items_.size()) * row_h_);
    rects.push_back(SDL_Rect{static_cast<int>(x_), static_cast<int>(y_),
                             static_cast<int>(width_), total_h});
    if (open_sub_ >= 0 && open_sub_ < static_cast<int>(items_.size())) {
        const int sub_h = static_cast<int>(static_cast<float>(items_[open_sub_].submenu.size()) *
                                           row_h_);
        rects.push_back(SDL_Rect{static_cast<int>(x_ + width_),
                                 static_cast<int>(y_ + static_cast<float>(open_sub_) * row_h_),
                                 static_cast<int>(sub_width_), sub_h});
    }
    return rects;
}

void Menu::close() {
    visible_ = false;
    hover_ = -1;
    hover_sub_ = -1;
    open_sub_ = -1;
}

bool Menu::contains(float x, float y) const {
    if (!visible_) {
        return false;
    }
    const float total_h = static_cast<float>(items_.size()) * row_h_;
    return x >= x_ && x <= x_ + width_ && y >= y_ && y <= y_ + total_h;
}

void Menu::on_motion(float x, float y) {
    if (!visible_) {
        return;
    }
    hover_ = -1;
    hover_sub_ = -1;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const SDL_FRect r = item_rect(static_cast<int>(i));
        if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
            hover_ = static_cast<int>(i);
            open_sub_ = items_[i].submenu.empty() ? -1 : static_cast<int>(i);
            update_submenu_geometry();
            break;
        }
    }
    if (open_sub_ >= 0) {
        const std::vector<MenuItem> &sub = items_[open_sub_].submenu;
        for (std::size_t i = 0; i < sub.size(); ++i) {
            const SDL_FRect r = submenu_item_rect(open_sub_, static_cast<int>(i));
            if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
                hover_sub_ = static_cast<int>(i);
                break;
            }
        }
    }
}

int Menu::on_click(float x, float y) {
    if (!visible_) {
        return -1;
    }
    if (!contains(x, y)) {
        // Maybe a click inside the open submenu.
        bool in_submenu = false;
        if (open_sub_ >= 0) {
            const std::vector<MenuItem> &sub = items_[open_sub_].submenu;
            for (std::size_t i = 0; i < sub.size(); ++i) {
                const SDL_FRect r = submenu_item_rect(open_sub_, static_cast<int>(i));
                if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
                    in_submenu = true;
                    if (sub[i].enabled && sub[i].kind != MenuItem::Kind::Separator) {
                        const int id = sub[i].id;
                        close();
                        return id;
                    }
                }
            }
        }
        if (!in_submenu) {
            close();
        }
        return -1;
    }

    const int index = hover_;
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        MenuItem &item = items_[index];
        if (item.kind == MenuItem::Kind::Separator || !item.enabled) {
            return -1;
        }
        if (!item.submenu.empty()) {
            open_sub_ = index;
            update_submenu_geometry();
            return -1;
        }
        const int id = item.id;
        close();
        return id;
    }
    return -1;
}

void Menu::render(SDL_Renderer *renderer, const TextRenderer &text) {
    if (!visible_) {
        return;
    }
    row_h_ = item_height(text);
    width_ = menu_width(text);
    sub_width_ = submenu_width(items_.empty() ? items_ : items_[0].submenu, text);
    if (open_sub_ >= 0) {
        sub_width_ = submenu_width(items_[open_sub_].submenu, text);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    auto draw_panel = [&](const SDL_FRect &r) {
        SDL_SetRenderDrawColor(renderer, kBg.r, kBg.g, kBg.b, kBg.a);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, kBorder.r, kBorder.g, kBorder.b, kBorder.a);
        SDL_FRect top{r.x, r.y, r.w, 1};
        SDL_FRect bot{r.x, r.y + r.h - 1, r.w, 1};
        SDL_FRect left{r.x, r.y, 1, r.h};
        SDL_FRect right{r.x + r.w - 1, r.y, 1, r.h};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    };

    const float total_h = static_cast<float>(items_.size()) * row_h_;
    draw_panel(SDL_FRect{x_, y_, width_, total_h});

    auto label_for = [](const MenuItem &item) {
        if (item.kind == MenuItem::Kind::Checkable) {
            return (item.checked ? std::string("[x] ") : std::string("[  ] ")) + item.label;
        }
        return item.label;
    };

    for (std::size_t i = 0; i < items_.size(); ++i) {
        const MenuItem &item = items_[static_cast<int>(i)];
        const SDL_FRect r = item_rect(static_cast<int>(i));
        if (item.kind == MenuItem::Kind::Separator) {
            SDL_SetRenderDrawColor(renderer, kBorder.r, kBorder.g, kBorder.b, 120);
            SDL_FRect line{r.x + 4, r.y + r.h * 0.5f, r.w - 8, 1};
            SDL_RenderFillRect(renderer, &line);
            continue;
        }
        if (static_cast<int>(i) == hover_) {
            SDL_SetRenderDrawColor(renderer, kHover.r, kHover.g, kHover.b, kHover.a);
            SDL_RenderFillRect(renderer, &r);
        }
        SDL_FRect tr{r.x + pad_, r.y, r.w - pad_ * 2.0f - 12.0f, r.h};
        text.draw(renderer, label_for(item), tr, item.enabled ? kText : kTextDim);
        if (!item.submenu.empty()) {
            SDL_FRect arrow{r.x + r.w - 16.0f, r.y + r.h * 0.5f - 1.0f, 8.0f, 2.0f};
            SDL_SetRenderDrawColor(renderer, kText.r, kText.g, kText.b, kText.a);
            SDL_RenderFillRect(renderer, &arrow);
        }
    }

    if (open_sub_ >= 0 && open_sub_ < static_cast<int>(items_.size())) {
        const std::vector<MenuItem> &sub = items_[open_sub_].submenu;
        const float sx = sub_x_;
        const float sy = sub_y_;
        const float sh = sub_h_;
        draw_panel(SDL_FRect{sx, sy, sub_width_, sh});
        for (std::size_t i = 0; i < sub.size(); ++i) {
            const MenuItem &item = sub[i];
            const SDL_FRect r = submenu_item_rect(open_sub_, static_cast<int>(i));
            if (item.kind == MenuItem::Kind::Separator) {
                SDL_SetRenderDrawColor(renderer, kBorder.r, kBorder.g, kBorder.b, 120);
                SDL_FRect line{r.x + 4, r.y + r.h * 0.5f, r.w - 8, 1};
                SDL_RenderFillRect(renderer, &line);
                continue;
            }
            if (static_cast<int>(i) == hover_sub_) {
                SDL_SetRenderDrawColor(renderer, kHover.r, kHover.g, kHover.b, kHover.a);
                SDL_RenderFillRect(renderer, &r);
            }
            SDL_FRect tr{r.x + pad_, r.y, r.w - pad_ * 2.0f, r.h};
            text.draw(renderer, label_for(item), tr, item.enabled ? kText : kTextDim);
        }
    }
}

}  // namespace de
