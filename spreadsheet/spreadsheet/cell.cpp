#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

#include "sheet.h"

using namespace std;

class Cell::Impl {
public:
	virtual ~Impl() = default;
	virtual std::string GetText() const = 0;
	virtual CellInterface::Value GetValue() const = 0;
	virtual std::vector<Position> GetReferencedCells() const {
		return {};
	}
};

FormulaError::FormulaError(Category category) : category_(category) {}

FormulaError::Category FormulaError::GetCategory() const {
	return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
	return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
	switch (category_) {
	case Category::Ref:
		return "#REF!";
	case Category::Value:
		return "#VALUE!";
	case Category::Arithmetic:
		return "#ARITHM!";
	default:
		return "#UNKNOWN!";
	}
}

class Cell::EmptyImpl : public Impl {
public:
	string GetText() const override {
		return ""s;
	}

	CellInterface::Value GetValue() const override {
		return ""s;
	}
};

class Cell::TextImpl : public Impl {
public:
	TextImpl(std::string text) : text_(move(text)) {}

	string GetText() const override {
		return text_;
	}

	CellInterface::Value GetValue() const override {
		if (text_.front() == ESCAPE_SIGN && !text_.empty()) {
			return text_.substr(1);
		}
		return text_;
	}

private:
	std::string text_;
};

class Cell::FormulaImpl : public Impl {
public:
	FormulaImpl(std::string text, Sheet& sheet)
		: formula_ptr_(ParseFormula(text.substr(1))), sheet_(sheet) {
	}

	std::string GetText() const override {
		return FORMULA_SIGN + formula_ptr_->GetExpression();
	}

	CellInterface::Value GetValue() const override {
		FormulaInterface::Value result = formula_ptr_->Evaluate(sheet_); // Используем sheet_
		if (std::holds_alternative<double>(result)) {
			return std::get<double>(result);
		}
		return std::get<FormulaError>(result);		
	}

	vector<Position> GetReferencedCells() const override {
		return formula_ptr_->GetReferencedCells();
	}

private:
	std::unique_ptr<FormulaInterface> formula_ptr_;
	Sheet& sheet_; // Ссылка на таблицу
};

// ------------- Cell ---------------------

// Реализуйте следующие методы
Cell::Cell(Sheet& sheet) : impl_(make_unique<EmptyImpl>()), sheet_(sheet) {}

Cell::~Cell() = default;

bool Cell::HasCycle(const std::vector<Cell*>& new_dependencies) const {
	std::unordered_set<const Cell*> visited;
	std::function<bool(const Cell*)> dfs = [&](const Cell* cell) {
		if (cell == this) {
			return true;
		} 
		if (visited.count(cell)) {
			return false;
		} 
		visited.insert(cell);
		for (const Cell* dependency : cell->dependecies_) {
			if (dfs(dependency)) {
				return true;
			} 
		}
		return false;
		};

	for (Cell* dep : new_dependencies) {
		if (dfs(dep)) {
			return true;
		}
	}
	return false;
}

void Cell::Set(std::string text) {
	unique_ptr<Impl> temp_impl;
	if (text.empty()) {
		temp_impl = make_unique<EmptyImpl>();
	}
	else if (text.front() == FORMULA_SIGN && text.size() > 1) {
		temp_impl = make_unique<FormulaImpl>(move(text), sheet_);
	}
	else {
		temp_impl = make_unique<TextImpl>(move(text));
	}

	// циклические зависимости 
	auto referenced_cells = temp_impl->GetReferencedCells();

	// Создание отсутствующих ячеек, делая их пустыми
	for (const auto& pos : referenced_cells) {
		if (!sheet_.GetCell(pos)) {
			sheet_.SetCell(pos, "");
		}
	}

	std::vector<Cell*> new_dependencies;
	for (const auto& pos : referenced_cells) {
		new_dependencies.push_back(static_cast<Cell*>(sheet_.GetCell(pos)));
	}

	if (HasCycle(new_dependencies)) {
		throw CircularDependencyException("Circular dependency detected");
	}

	// Обновление графа
	for (Cell* dependency : dependecies_) {
		dependency->references_.erase(this);
	}
	dependecies_.clear();
	for (Cell* new_dep : new_dependencies) {
		new_dep->references_.insert(this);
		dependecies_.insert(new_dep);
	}

	impl_ = std::move(temp_impl);
	InvalidateCache();
}

void Cell::Clear() {
	impl_ = make_unique<EmptyImpl>();
}

Cell::Value Cell::GetValue() const {
	return impl_->GetValue();
}

std::string Cell::GetText() const {
	return impl_->GetText();
}

bool Cell::IsReferenced() const {
	return !references_.empty();
}

bool Cell::IsCacheValid() const {
	return cached_value.has_value();
}

Cell* Cell::GetCellByPos(Position pos) const {
	return static_cast<Cell*>(sheet_.GetCell(pos));
}

void Cell::InvalidateCache() const {
	if (cached_value.has_value()) {
		cached_value.reset();
		for (const auto& dep_cell : dependecies_) {		
			if (dep_cell) {
				dep_cell->InvalidateCache();
			}
		}
	}
}

void Cell::AddDepency(const Position current_pos) {	// Внести позицию текущей ячейки
	// Будем добавлять dependencies по одной из вектора references зависимой ячейки. Метод переписать
	for (auto& current_cell_ptr : references_) {		
		current_cell_ptr->AddDepency(current_pos);
	}
}

vector<Position> Cell::GetReferencedCells() const {
	if (!impl_) {
		return {};
	}
	return impl_->GetReferencedCells();
}