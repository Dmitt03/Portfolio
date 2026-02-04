#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include "cell.h"

using namespace std;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
    class Formula : public FormulaInterface {
    public:
        // Реализуйте следующие методы:
        explicit Formula(std::string expression)
            : ast_(ParseFormulaAST(expression)) {
        }

        std::vector<Position> GetReferencedCells() const override {
            auto cells = ast_.GetCells();
            std::vector<Position> result(cells.begin(), cells.end());
            std::sort(result.begin(), result.end());
            auto last = std::unique(result.begin(), result.end());
            result.erase(last, result.end());
            return result;
        }


        Value Evaluate(const SheetInterface& sheet) const override {
            try {
                function<double(Position)> args = [&sheet](Position pos) -> double {
                    if (!pos.IsValid()) {
                        throw FormulaError(FormulaError::Category::Ref);
                    }
                    const CellInterface* cell = sheet.GetCell(pos);
                    if (!cell) {
                        return 0.0;
                    }
                    const auto& value = cell->GetValue();
                    if (std::holds_alternative<double>(value)) {
                        return std::get<double>(value);
                    }
                    if (std::holds_alternative<std::string>(value)) {
                        const std::string& str = std::get<std::string>(value);
                        if (str.empty()) {
                            return 0.0;
                        }
                        try {
                            size_t pos = 0;
                            double result = std::stod(str, &pos);                            
                            if (pos != str.size()) {
                                throw FormulaError(FormulaError::Category::Value);
                            }
                            return result;
                        }
                        catch (...) {
                            throw FormulaError(FormulaError::Category::Value);
                        }
                    }
                    throw std::get<FormulaError>(value);
                    };
                return ast_.Execute(args);
            }
            catch (const FormulaError& e) {
                return e;
            }
        }

        std::string GetExpression() const override {
            std::ostringstream oss;
            ast_.PrintFormula(oss);
            return oss.str();
        }

    private:
        FormulaAST ast_;
    };
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    }
    catch (const std::exception& e) {
        throw FormulaException(e.what());
    }
    catch (...) {
        throw FormulaException("Unknown error");
    }
}