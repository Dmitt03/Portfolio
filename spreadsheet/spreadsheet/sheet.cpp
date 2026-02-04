#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std;

Sheet::~Sheet() {}

bool Sheet::IsEmpty(Position pos) const {
    if (pos.row >= static_cast<int>(cells_.size())) {
        return true;
    }
    if (pos.col >= static_cast<int>(cells_[pos.row].size())) {
        return true;
    }
    if (!cells_[pos.row][pos.col]) {
        return true;
    }
    if (cells_[pos.row][pos.col]->GetText().empty()) {
        return true;
    }

    return false;
}

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position!");
    }
    if (pos.row >= static_cast<int>(cells_.size())) {
        cells_.resize(pos.row + 1);
    }
    if (pos.col >= static_cast<int>(cells_[pos.row].size())) {
        cells_[pos.row].resize(pos.col + 1);
    }
    if (!cells_[pos.row][pos.col]) {
        cells_[pos.row][pos.col] = make_unique<Cell>(*this);
    }    
    cells_[pos.row][pos.col]->Set(move(text));
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position!");
    }

    if (pos.row >= static_cast<int>(cells_.size())) {
        return nullptr;
    }

    if (pos.col >= static_cast<int>(cells_[pos.row].size())) {
        return nullptr;
    }

    return cells_[pos.row][pos.col].get();
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position!");
    }

    if (pos.row >= static_cast<int>(cells_.size())) {
        return nullptr;
    }

    if (pos.col >= static_cast<int>(cells_[pos.row].size())) {
        return nullptr;
    }

    return cells_[pos.row][pos.col].get();
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position!");
    }
    if (IsEmpty(pos)) {
        return;
    }
    cells_[pos.row][pos.col].reset();
}

Size Sheet::GetPrintableSize() const {
    int row = -1;
    int col = -1;

    for (size_t i = 0; i < cells_.size(); ++i) {
        for (size_t j = 0; j < cells_[i].size(); ++j) {
            if (cells_[i][j] && !cells_[i][j]->GetText().empty()) {
                row = i;
                col = max(col, int(j));
            }
        }
    }
    
    if (row == -1) {
        return { 0, 0 };
    }

    return { row + 1, col + 1 };
}

void operator<<(ostream& os, CellInterface::Value value) {
    if (holds_alternative<string>(value)) {
        os << get<string>(value);
    } else if (holds_alternative<double>(value)) {
        os << get<double>(value);
    } else {
        os << get<FormulaError>(value);
    }
}

void Sheet::PrintValues(std::ostream& output) const {
    const Size size = GetPrintableSize();
    if (size.rows == 0 || size.cols == 0) {
        return;
    }
    for (int i = 0; i < size.rows; ++i) {
        if (IsEmpty({ i, 0})) {
            output << "";
        }
        else {
            output << cells_[i][0]->GetValue();
        }

        for (int j = 1; j < size.cols; ++j) {   
            output << '\t';
            if (IsEmpty({ i,j })) {
                output << "";
            } else {
                output << cells_[i][j]->GetValue();
            }            
        }
        output << '\n';
    }
}
void Sheet::PrintTexts(std::ostream& output) const {
    const Size size = GetPrintableSize();
    if (size.rows == 0 || size.cols == 0) {
        return;
    }
    for (int i = 0; i < size.rows; ++i) {
        if (IsEmpty({ i, 0 })) {
            output << "";
        }
        else {
            output << cells_[i][0]->GetText();
        }

        for (int j = 1; j < size.cols; ++j) {
            output << '\t';
            if (IsEmpty({ i,j })) {
                output << "";
            }
            else {
                output << cells_[i][j]->GetText();
            }
        }
        output << '\n';
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}