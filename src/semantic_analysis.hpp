#pragma once

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctype.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfg.hpp"
#include "color.hpp"
#include "err.hpp"
#include "re.hpp"
#include "scan.hpp"
#include "expr.hpp"

struct SemanticAnalysis {
    SemanticAnalysis(std::vector<Expr*>& stmts): stmts(stmts) { }

    void analyse(){
        //for (const auto stmtexpr : stmts){
        //    stmtexpr->visit();
        //}
    }

    std::vector<Expr*>& stmts;
};
