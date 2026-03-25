#include "cell.h"
#include "sheet.h" 
#include <cassert>
#include <iostream>
#include <string>

class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual std::string GetText() const = 0;
    // Передаем sheet сюда, чтобы избежать ошибки со статической ссылкой
    virtual Cell::Value GetValue(const SheetInterface& sheet) const = 0;
    virtual std::vector<Position> GetReferencedCells() const { 
        return {}; 
    }
};

class Cell::EmptyImpl : public Cell::Impl {
public:
    std::string GetText() const override { 
        return ""; 
    }
    Cell::Value GetValue(const SheetInterface&) const override { 
        return ""; 
    }
};

class Cell::TextImpl : public Cell::Impl {
public:
    explicit TextImpl(std::string text) : text_(std::move(text)) {}
    std::string GetText() const override { 
        return text_; 
    }
    Cell::Value GetValue(const SheetInterface&) const override {
        if (!text_.empty() && text_.front() == '\'') {
            return text_.substr(1);
        }
        return text_;
    }
private:
    std::string text_;
};

class Cell::FormulaImpl : public Cell::Impl {
public:
    explicit FormulaImpl(std::string text) : formula_(ParseFormula(std::move(text))) {}
    std::string GetText() const override { 
        return "=" + formula_->GetExpression(); 
    }
    Cell::Value GetValue(const SheetInterface& sheet) const override {
        auto val = formula_->Evaluate(sheet);
        if (std::holds_alternative<double>(val)) return std::get<double>(val);
        if (std::holds_alternative<FormulaError>(val)) return std::get<FormulaError>(val);
        return "";
    }
    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }
private:
    std::unique_ptr<FormulaInterface> formula_;
};

Cell::Cell(Sheet& sheet, Position pos)
    : impl_(std::make_unique<EmptyImpl>()), sheet_(sheet), pos_(pos) {
}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> new_impl;
    std::vector<Position> new_refs;

    if (text.empty()) {
        new_impl = std::make_unique<EmptyImpl>();
    }
    else if (text.front() == '=' && text.size() > 1) {
        new_impl = std::make_unique<FormulaImpl>(text.substr(1));
        new_refs = new_impl->GetReferencedCells();
    }
    else {
        new_impl = std::make_unique<TextImpl>(std::move(text));
    }

    // Проверяем граф на циклы до применения изменений
    CheckCircularDependencies(new_refs);

    ClearDependencies();
    impl_ = std::move(new_impl);
    UpdateDependencies(new_refs);
    InvalidateCache();
}

void Cell::Clear() {
    Set("");
}

Cell::Value Cell::GetValue() const {
    if (cache_.has_value()) {
        return cache_.value();
    }
    cache_ = impl_->GetValue(sheet_);
    return cache_.value();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !dependent_cells_.empty();
}

void Cell::InvalidateCache() {
    if (cache_.has_value()) {
        cache_ = std::nullopt;
        for (Cell* dep : dependent_cells_) {
            dep->InvalidateCache();
        }
    }
}

void Cell::ClearDependencies() {
    for (Cell* ref : referenced_cells_) {
        ref->dependent_cells_.erase(this);
    }
    referenced_cells_.clear();
}

void Cell::UpdateDependencies(const std::vector<Position>& new_refs) {
    for (Position pos : new_refs) {
        Cell* ref = sheet_.GetConcreteCell(pos);
        if (!ref) {
            // Если ячейки нет, создаем пустую, чтобы завязать граф
            sheet_.SetCell(pos, "");
            ref = sheet_.GetConcreteCell(pos);
        }
        referenced_cells_.insert(ref);
        ref->dependent_cells_.insert(this);
    }
}

void Cell::CheckCircularDependencies(const std::vector<Position>& new_refs) const {
    std::unordered_set<Position, PositionHasher> visited;
    std::vector<Position> stack = new_refs;

    while (!stack.empty()) {
        Position curr = stack.back();
        stack.pop_back();

        if (curr == this->pos_) {
            throw CircularDependencyException("Circular dependency detected");
        }

        if (visited.find(curr) == visited.end()) {
            visited.insert(curr);
            const auto* cell = sheet_.GetCell(curr);
            if (cell) {
                for (Position ref : cell->GetReferencedCells()) {
                    stack.push_back(ref);
                }
            }
        }
    }
}