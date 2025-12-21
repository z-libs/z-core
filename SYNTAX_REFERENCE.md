# zdoc_gen Syntax Reference

## 1. Structure & Organization
| Directive | Markdown Output | Description |
|---|---|---|
| `/// @toc` | (Links) | Generates a Table of Contents for all sections. |
| `/// @section Title` | `## Title` | Starts a new main section. |
| `/// @subsection Title` | `### Title` | Starts a subsection. |
| `/// @group Title` | `## Title` + Table | Starts a section and opens a Function table. |
| `/// @endgroup` | N/A | Closes the current group/table mode. |
| `/// @separator` | `---` | Inserts a horizontal rule. |

## 2. Data Types
| Directive | Description |
|---|---|
| `/// @struct Name` | Starts a table for struct fields. Looks for `type var;`. |
| `/// @enum Name` | Starts a table for enum values. Looks for `VAL,`. |
| **Note** | Doxygen style trailing comments `//< desc` are supported. |

## 3. Function/Member Documentation
| Directive | Usage |
|---|---|
| `/// @param name Desc` | Adds `**Param**: name Desc` to the table cell. |
| `/// @return Desc` | Adds `**Returns**: Desc` to the table cell. |
| `/// @private` | Skips the next function/variable completely. |
| `/// @skip` | Skips the next line of code (but keeps docs pending). |

## 4. Rich Content
| Directive | Description |
|---|---|
| `/// @example [lang]` | Starts a code block. Inside tables, this becomes `**Example**: <code>`. |
| `/// @endexample` | Ends the code block. |
| `/// @include file.txt` | Injects the raw contents of `file.txt` at this position. |
| `/// @raw <html>` | Injects raw HTML without escaping. |

## 5. Alerts & Metadata
| Directive | Output |
|---|---|
| `/// @warn Msg` | `**Warning**: Msg` |
| `/// @note Msg` | `**Note**: Msg` |
| `/// @deprecated` | Marks the item as **Deprecated**. |
| `/// @bug Msg` | Adds an entry to the **Known Issues** section at the bottom. |
| `/// @todo Msg` | Adds an entry to the **Known Issues** section at the bottom. |

## 6. Table Management (Manual)
| Directive | Description |
|---|---|
| `/// @table Title` | Starts a new table with the given title. |
| `/// @columns A | B` | Sets custom column headers for the next table. |

## Injection Markers
In your `README.in`, use these markers:

- `[//]: # (ZDOC_START)`
- `[//]: # (ZDOC_END)`

