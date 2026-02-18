#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>
#include <optional>
#include <memory>
#include <vector>

class Sheet;

struct PositionHasher {
    size_t operator()(const Position& pos) const {
        return std::hash<int>()(pos.row) ^ (std::hash<int>()(pos.col) << 1);
    }
};

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

    bool IsCacheValid() const;
    void InvalidateCache() const;    

    // Возвращает ячейки или кидает ошибку
    void AddDepency(const Position current_pos);

    Cell* GetCellByPos(Position pos) const;

    bool HasCycle(const std::vector<Cell*>& new_dependencies) const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;

    std::unordered_set<Cell*> references_;
    std::unordered_set<Cell*> dependecies_;

    mutable std::optional<FormulaInterface::Value> cached_value;
    Sheet& sheet_;
};

    // Добавьте поля и методы для связи с таблицей, проверки циклических 
    // зависимостей, графа зависимостей и т. д