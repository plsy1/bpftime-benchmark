# Control input for tools/check_quotes.py (run_all.sh expects exactly 2 fragments rejected: one altered word, one wrong section)

Unchanged quote (must be found):
> [ptx-isa §10.4] "The warp identifier provides a unique warp number within a CTA but not across CTAs within a grid."

Altered quote, one word changed ("within a CTA" -> "within an SM"; must NOT be found):
> [ptx-isa §10.4] "The warp identifier provides a unique warp number within an SM but not across CTAs within a grid."

Unchanged quote from another document, in a table cell (must be found):
| x | [cpg-cpp-language-extensions §5.4.6.6] "All non-exited threads specified in the mask must execute the intrinsic with the same mask value." | y |

Unchanged quote under a wrong section number (§10.5 instead of §10.4; must be flagged as WRONG SECTION):
> [ptx-isa §10.5] "The warp identifier will be the same for all threads within a single warp."
