/**
 * @name Mockturle header included via system path
 * @description Avoid including mockturtle headers using system include paths.
                Use the relative local include path instead. For instance, if
                you want to include `mockturtle/X/Y.hpp` from
                `mockturtle/A/B.hpp` then use ``#include "../X/Y.hpp"`.
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id cpp/mockturtle
 * @tags mockturtle
 */

import cpp

from Include i
where i.getIncludeText().regexpMatch("<mockturtle/[a-z0-9_/]+.hpp>") and
      i.getFile().toString().matches("%/mockturtle/include/%")
select i, "Use local include path instead of system include path."
