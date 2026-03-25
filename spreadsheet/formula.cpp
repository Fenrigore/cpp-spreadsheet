#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <cmath>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
class Formula : public FormulaInterface {
public:
// Реализуйте следующие методы:
    explicit Formula(std::string expression) try
    : ast_(ParseFormulaAST(std::move(expression))) {
    }catch (const FormulaException&) { //ошибка в формуле
        throw; //передаем дальше
    }
    catch (const std::exception& e) {
        throw FormulaException(e.what());
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        auto get_cell_value = [&sheet](Position pos) -> double {
            if (!pos.IsValid()) {
                throw FormulaError(FormulaError::Category::Ref);
            }

            const auto* cell = sheet.GetCell(pos);
            // Если ячейки нет или она пустая (GetCell вернул nullptr), по правилам это 0
            if (!cell) { 
                return 0.0; 
            }

            auto val = cell->GetValue();

            // Вариант А: В ячейке уже лежит число (double)
            if (const double* p_val = std::get_if<double>(&val)) {
                return *p_val;
            }

            // Вариант Б: В ячейке текст (string) — пытаемся конвертировать в число
            if (const std::string* p_str = std::get_if<std::string>(&val)) {
                if (p_str->empty()) { 
                    return 0.0; 
                }

                std::istringstream in(*p_str);
                double res = 0;
                // Считываем число. Если считалось — проверяем, не осталось ли чего-то лишнего
                if (!(in >> res)) {
                    throw FormulaError(FormulaError::Category::Value);
                }
                // Проверяем на наличие "мусора" после числа (кроме пробелов)
                char remainder;
                if (in >> remainder) {
                    throw FormulaError(FormulaError::Category::Value);
                }
                return res;
            }

            // Вариант В: В ячейке уже была ошибка (FormulaError) — прокидываем её вверх
            throw std::get<FormulaError>(val);
            };

        try {
            // 2. Выполняем вычисление через AST
            double result = ast_.Execute(get_cell_value);

            // 3. Финальная проверка на математическую корректность (inf, nan)
            if (!std::isfinite(result)) {
                return FormulaError(FormulaError::Category::Arithmetic);
            }

            return result;
        }
        catch (const FormulaError& fe) {
            // Если на любом этапе выше была брошена ошибка, возвращаем её как результат
            return fe;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> res;
        for (auto pos : ast_.GetCells()) {
            res.push_back(pos);
        }
        return res;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}