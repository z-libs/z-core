
#define ZSTR_IMPLEMENTATION
#include "zstr.h"
#define ZFILE_IMPLEMENTATION
#include "zfile.h"
#include <ctype.h>

const char *DEFAULT_OUT = "README.md";
const char *MARKER_START = "[//]: # (ZDOC_START)";
const char *MARKER_END   = "[//]: # (ZDOC_END)";

typedef enum { 
    MODE_NONE, 
    MODE_TEXT, 
    MODE_TABLE,   // Functions.
    MODE_STRUCT,  // Fields.
    MODE_ENUM     // Values.
} DocMode;

// Helpers.

bool is_code_decl(zstr_view line, DocMode mode) 
{
    if (0 == line.len) 
    {
        return false;
    }
    if (zstr_view_starts_with(line, "//") || 
        zstr_view_starts_with(line, "/*")) 
    {
            return false;
    }
    if ('}' == line.data[0]) 
    {
        return false;
    }
    if ('#' == line.data[0]) 
    {
        return zstr_view_starts_with(line, "#define");
    }
    if (MODE_TABLE == mode) 
    {
        for(size_t i = 0; i < line.len; i++) 
        {
            if ('(' == line.data[i]) 
            {
                return true;
            }
        }
        return false;
    }
    if (MODE_STRUCT == mode) 
    {
        return ';' == line.data[line.len - 1];
    }
    if (MODE_ENUM == mode) 
    {
        if (zstr_view_starts_with(line, "typedef")) 
        {
            return false;
        }
        if (zstr_view_starts_with(line, "enum")) 
        {
            return false;
        }
        for(size_t i = 0; i < line.len; i++) 
        {
            if ('{' == line.data[i]) 
            {
                return false;
            }
        }
        return true; 
    }
    return false;
}

bool is_incomplete(zstr_view line, DocMode mode) 
{
    zstr_view v = zstr_view_trim(line);
    if (0 == v.len) 
    {
        return false;
    }
    if (zstr_view_starts_with(v, "#")) 
    {
        return '\\' == v.data[v.len - 1];
    }
    char last = v.data[v.len - 1];
    if (MODE_TABLE == mode) 
    {
        return (last != ';' && last != '{');
    }
    if (MODE_ENUM == mode) 
    {
        return false;
    }
    return (last != ';' && last != '{' && last != ',');
}

zstr clean_signature(zstr_view line) 
{
    zstr s = zstr_from_view(line);
    zstr_trim(&s); 
    if (zstr_ends_with(&s, ";") || 
        zstr_ends_with(&s, "{") || 
        zstr_ends_with(&s, ",")) 
    {
        zstr_pop_char(&s);
    }
    zstr_replace(&s, "\\", ""); 
    zstr_replace(&s, "static inline ", "");
    zstr_replace(&s, "extern ", "");
    zstr_replace(&s, "Z_NODISCARD ", "");
    zstr_replace(&s, "typedef ", "");
    zstr_trim(&s);
    return s;
}

zstr_view strip_decorative_star(zstr_view v) 
{
    v = zstr_view_trim(v);
    while (v.len > 0 && '*' == v.data[0]) 
    {
        v = zstr_view_trim(zstr_sub(v, 1, v.len - 1));
    }
    return v;
}

zstr make_anchor(zstr_view title) 
{
    zstr s = zstr_init();
    zstr_push(&s, '#');
    for (size_t i = 0; i < title.len; i++) 
    {
        char c = tolower(title.data[i]);
        if (isalnum(c) || '-' == c || '_' == c) 
        {
            zstr_push(&s, c);
        }
        else if (' ' == c) 
        {
            zstr_push(&s, '-');
        }
    }
    return s;
}

int main(int argc, char **argv) 
{
    if (argc < 2) 
    {
        printf("Usage: zdoc_gen <header> [output] [template]\n");
        return 1;
    }

    const char *input_path = argv[1];
    const char *out_path = (argc > 2) ? argv[2] : DEFAULT_OUT;
    const char *tpl_path = (argc > 3) ? argv[3] : NULL;

    if (!zfile_exists(input_path)) 
    {
        return 1;
    }
    
    zstr doc_content = zstr_init();
    zstr toc_content = zstr_init();
    zstr issues_content = zstr_init();
    zstr pending_desc = zstr_init();
    zstr current_columns = zstr_from("Function | Description");
    
    DocMode mode = MODE_TEXT; 
    bool in_block_comment = false;
    bool in_example = false;
    bool skip_next = false;
    bool is_private = false;

    zfile_reader reader = zfile_reader_open(input_path);
    zstr_view raw_line;

    while (zfile_reader_next_line(&reader, &raw_line)) 
    {
        zstr_view line = zstr_view_trim(raw_line);
        if (0 == line.len && !in_block_comment) 
        {
            continue;
        }

        zstr_view content = {NULL, 0};
        zstr_view raw_content = {NULL, 0};
        bool is_doc_line = false;

        if (!in_block_comment && zstr_view_starts_with(line, "///")) 
        {
            raw_content = zstr_sub(line, 3, line.len - 3);
            content = zstr_view_trim(raw_content);
            is_doc_line = true;
        } 
        else if (!in_block_comment && zstr_view_starts_with(line, "/**")) 
        {
            in_block_comment = true;
            size_t offset = (line.len > 2 && '*' == line.data[2]) ? 3 : 2;
            content = zstr_view_trim(zstr_sub(line, offset, line.len - offset));
            if (zstr_view_ends_with(content, "*/")) 
            {
                content.len -= 2; 
                content = zstr_view_trim(content); 
                in_block_comment = false;
            }
            content = strip_decorative_star(content);
            is_doc_line = true;
        } 
        else if (in_block_comment) 
        {
            content = line;
            if (zstr_view_ends_with(content, "*/")) 
            {
                content.len -= 2; 
                in_block_comment = false;
            }
            content = strip_decorative_star(content);
            is_doc_line = true;
        }

        if (is_doc_line) 
        {
            if (in_example) 
            {
                if (zstr_view_starts_with(content, "@endexample")) 
                {
                    zstr *target = (MODE_TEXT != mode) ? &pending_desc : &doc_content;
                    if (MODE_TEXT != mode) 
                    {
                        zstr_cat(target, "</code>");
                    }
                    else 
                    {
                        zstr_cat(target, "```\n");
                    }
                    in_example = false;
                } 
                else 
                {
                    zstr_view code_line = raw_content;
                    if (code_line.len > 0 && ' ' == code_line.data[0])
                    {
                        code_line = zstr_sub(code_line, 1, code_line.len - 1);
                    }
                    zstr *target = (MODE_TEXT != mode) ? &pending_desc : &doc_content;
                    if (MODE_TEXT != mode) 
                    {
                        if (!zstr_ends_with(target, "<code>")) 
                        {
                            zstr_push(target, ' ');
                        }
                         zstr_cat_len(target, code_line.data, code_line.len);
                    } 
                    else 
                    {
                        zstr_cat_len(target, code_line.data, code_line.len);
                        zstr_push(target, '\n');
                    }
                }
                continue;
            }

            if (zstr_view_starts_with(content, "@section") || 
                zstr_view_starts_with(content, "@group")) 
            {
                bool is_group = zstr_view_starts_with(content, "@group");
                zstr_view title = zstr_view_trim(zstr_sub(content, (is_group ? 6 : 8), content.len - (is_group ? 6 : 8)));
                zstr_fmt(&doc_content, "\n## %.*s\n\n", ZSV_ARG(title));
                zstr anchor = make_anchor(title);
                zstr_fmt(&toc_content, "* [%.*s](%s)\n", ZSV_ARG(title), zstr_cstr(&anchor));
                zstr_free(&anchor);
                if (is_group) 
                {
                    zstr_fmt(&doc_content, "| Function | Description |\n|---|---|\n");
                    mode = MODE_TABLE; 
                } 
                else 
                {
                    mode = MODE_TEXT; 
                }
                zstr_clear(&pending_desc);
            }
            else if (zstr_view_starts_with(content, "@subsection")) 
            {
                zstr_view title = zstr_view_trim(zstr_sub(content, 11, content.len - 11));
                zstr_fmt(&doc_content, "\n### %.*s\n\n", ZSV_ARG(title));
                mode = MODE_TEXT; 
                zstr_clear(&pending_desc);
            }
            else if (zstr_view_starts_with(content, "@endgroup")) 
            { 
                mode = MODE_TEXT; 
            }
            else if (zstr_view_starts_with(content, "@struct")) 
            {
                zstr_view name = zstr_view_trim(zstr_sub(content, 7, content.len - 7));
                zstr_fmt(&doc_content, "\n### struct %.*s\n\n", ZSV_ARG(name));
                zstr_cat(&doc_content, "| Field | Type/Description |\n|---|---|\n");
                mode = MODE_STRUCT; 
                zstr_clear(&pending_desc);
            }
            else if (zstr_view_starts_with(content, "@enum")) 
            {
                zstr_view name = zstr_view_trim(zstr_sub(content, 5, content.len - 5));
                zstr_fmt(&doc_content, "\n### enum %.*s\n\n", ZSV_ARG(name));
                zstr_cat(&doc_content, "| Value | Description |\n|---|---|\n");
                mode = MODE_ENUM; zstr_clear(&pending_desc);
            }
            else if (zstr_view_starts_with(content, "@table")) 
            {
                zstr_view title = zstr_view_trim(zstr_sub(content, 6, content.len - 6));
                zstr_fmt(&doc_content, "\n**%.*s**\n\n", ZSV_ARG(title));
                zstr_fmt(&doc_content, "| %s |\n", zstr_cstr(&current_columns));
                zstr_cat(&doc_content, "|");
                zstr_view cols_v = zstr_view_from(zstr_cstr(&current_columns));
                zstr_split_iter it = zstr_split_init(cols_v, "|");
                zstr_view part;
                while(zstr_split_next(&it, &part)) 
                {
                    zstr_cat(&doc_content, "---|");
                }
                zstr_cat(&doc_content, "\n");
                mode = MODE_TABLE; zstr_clear(&pending_desc);
            }
            else if (zstr_view_starts_with(content, "@columns")) 
            {
                zstr_view cols = zstr_view_trim(zstr_sub(content, 8, content.len - 8));
                zstr_free(&current_columns); 
                current_columns = zstr_from_view(cols);
            }
            else if (zstr_view_starts_with(content, "@row")) 
            {
                zstr_view row = zstr_view_trim(zstr_sub(content, 4, content.len - 4));
                zstr_fmt(&doc_content, "| %.*s |\n", ZSV_ARG(row));
            }
            else if (zstr_view_starts_with(content, "@separator")) 
            {
                zstr_cat(&doc_content, "\n---\n");
            }
            else if (zstr_view_starts_with(content, "@skip")) 
            {
                skip_next = true;
            }
            else if (zstr_view_starts_with(content, "@private")) {
                is_private = true;
            }
            else if (zstr_view_starts_with(content, "@deprecated")) 
            {
                if (zstr_len(&pending_desc) > 0) 
                {
                    zstr_cat(&pending_desc, "<br>");
                }
                zstr_cat(&pending_desc, "**Deprecated**");
            }
            else if (zstr_view_starts_with(content, "@param")) 
            {
                zstr_view param = zstr_view_trim(zstr_sub(content, 6, content.len - 6));
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(&pending_desc) > 0) 
                    {
                        zstr_cat(&pending_desc, "<br>");
                    }
                    zstr_fmt(&pending_desc, "**Param**: %.*s", ZSV_ARG(param));
                }
            }
            else if (zstr_view_starts_with(content, "@return")) 
            {
                zstr_view ret = zstr_view_trim(zstr_sub(content, 7, content.len - 7));
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(&pending_desc) > 0) 
                    {
                        zstr_cat(&pending_desc, "<br>");
                    }
                    zstr_fmt(&pending_desc, "**Returns**: %.*s", ZSV_ARG(ret));
                }
            }
            else if (zstr_view_starts_with(content, "@warn")) 
            {
                zstr_view msg = zstr_view_trim(zstr_sub(content, 5, content.len - 5));
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(&pending_desc) > 0) 
                    {
                        zstr_cat(&pending_desc, "<br>");
                    }
                    zstr_fmt(&pending_desc, "**Warning**: %.*s", ZSV_ARG(msg));
                } 
                else 
                {
                    zstr_fmt(&doc_content, "> **Warning**: %.*s\n", ZSV_ARG(msg));
                }
            }
            else if (zstr_view_starts_with(content, "@note")) 
            {
                zstr_view msg = zstr_view_trim(zstr_sub(content, 5, content.len - 5));
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(&pending_desc) > 0) 
                    {
                        zstr_cat(&pending_desc, "<br>");
                    }
                    zstr_fmt(&pending_desc, "**Note**: %.*s", ZSV_ARG(msg));
                } 
                else 
                {
                    zstr_fmt(&doc_content, "> **Note**: %.*s\n", ZSV_ARG(msg));
                }
            }
            else if (zstr_view_starts_with(content, "@todo")) 
            {
                zstr_view msg = zstr_view_trim(zstr_sub(content, 5, content.len - 5));
                zstr_fmt(&issues_content, "* [TODO] %.*s\n", ZSV_ARG(msg));
            }
            else if (zstr_view_starts_with(content, "@bug")) 
            {
                zstr_view msg = zstr_view_trim(zstr_sub(content, 4, content.len - 4));
                zstr_fmt(&issues_content, "* [BUG] %.*s\n", ZSV_ARG(msg));
            }
            else if (zstr_view_starts_with(content, "@include")) 
            {
                zstr_view path = zstr_view_trim(zstr_sub(content, 8, content.len - 8));
                zstr inc_data = zfile_read_all(zstr_from_view(path).s.buf); 
                zstr_cat(&doc_content, zstr_cstr(&inc_data)); 
                zstr_push(&doc_content, '\n'); 
                zstr_free(&inc_data);
            }
            else if (zstr_view_starts_with(content, "@raw")) 
            {
                zstr_view raw = zstr_sub(content, 4, content.len - 4);
                zstr_cat_len(&doc_content, raw.data, raw.len); 
                zstr_push(&doc_content, '\n');
            }
            else if (zstr_view_starts_with(content, "@example")) 
            {
                zstr_view lang = zstr_view_trim(zstr_sub(content, 8, content.len - 8));
                if (0 == lang.len) 
                {
                    lang = zstr_view_from("c"); 
                }
                zstr *target = (MODE_TEXT != mode) ? &pending_desc : &doc_content;
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(target) > 0) 
                    {
                        zstr_cat(target, "<br>");
                    }
                    zstr_cat(target, "**Example**: <code>");
                } 
                else 
                {
                    zstr_fmt(target, "\n```%.*s\n", ZSV_ARG(lang));
                }
                in_example = true;
            }
            else 
            {
                if (MODE_TEXT != mode) 
                {
                    if (zstr_len(&pending_desc) > 0) 
                    {
                        zstr_push(&pending_desc, ' ');
                    }
                    zstr_cat_len(&pending_desc, content.data, content.len);
                } 
                else 
                {
                    zstr_fmt(&doc_content, "%.*s\n", ZSV_ARG(content));
                }
            }
            continue;
        }

        if (MODE_TEXT != mode && !in_block_comment) 
        {
            if (skip_next || is_private) 
            { 
                skip_next = false; 
                is_private = false; 
                zstr_clear(&pending_desc); 
                continue; 
            }
            zstr_split_iter it = zstr_split_init(line, "//");
            zstr_view code_part = {0};
            zstr_split_next(&it, &code_part);
            code_part = zstr_view_trim(code_part);
            if (!is_code_decl(code_part, mode)) 
            {
                continue;
            }
            zstr full_decl = zstr_from_view(code_part);
            while (is_incomplete(zstr_as_view(&full_decl), mode)) 
            {
                zstr_view next_line_v;
                if (!zfile_reader_next_line(&reader, &next_line_v)) 
                {
                    break;
                }
                next_line_v = zstr_view_trim(next_line_v);
                if (0 == next_line_v.len) 
                {
                    continue; 
                }
                zstr_push(&full_decl, ' ');
                zstr_cat_len(&full_decl, next_line_v.data, next_line_v.len);
            }
            if (zstr_starts_with(&full_decl, "#define") && zstr_is_empty(&pending_desc)) 
            { 
                zstr_free(&full_decl); 
                continue; 
            }
            if (zstr_is_empty(&pending_desc)) 
            {
                zstr_view trailing;
                if (zstr_split_next(&it, &trailing)) 
                {
                    trailing = zstr_view_trim(trailing);
                    if (trailing.len > 0 && '<' == trailing.data[0]) 
                    {
                        trailing = zstr_view_trim(zstr_sub(trailing, 1, trailing.len - 1));
                    }
                    zstr_cat_len(&pending_desc, trailing.data, trailing.len);
                }
            }
            zstr clean_sig = clean_signature(zstr_as_view(&full_decl));
            zstr_fmt(&doc_content, "| `%s` | %s |\n", zstr_cstr(&clean_sig), zstr_cstr(&pending_desc));
            zstr_free(&clean_sig); 
            zstr_free(&full_decl); 
            zstr_clear(&pending_desc);
        }
    }
    zfile_reader_close(&reader);

    zstr generated = zstr_init();
    if (strstr(zstr_data(&doc_content), "@toc")) 
    {
        zstr_replace(&doc_content, "@toc", "");
        zstr_cat(&generated, "## Table of Contents\n");
        zstr_cat(&generated, zstr_cstr(&toc_content));
        zstr_cat(&generated, "\n");
    }
    zstr_cat(&generated, zstr_cstr(&doc_content));
    if (zstr_len(&issues_content) > 0) 
    {
        zstr_cat(&generated, "\n## Known Issues\n");
        zstr_cat(&generated, zstr_cstr(&issues_content));
    }

    zstr existing = zstr_init();
    bool loaded = false;
    if (tpl_path && zfile_exists(tpl_path)) 
    { 
        existing = zfile_read_all(tpl_path); 
        loaded = true; 
    } 
    else if (zfile_exists(out_path)) 
    { 
        existing = zfile_read_all(out_path); 
        loaded = true; 
    }

    zstr final_out = zstr_init();
    char *start_marker = loaded ? strstr(zstr_data(&existing), MARKER_START) : NULL;
    char *end_marker   = loaded ? strstr(zstr_data(&existing), MARKER_END) : NULL;
    if (start_marker && end_marker && end_marker > start_marker) 
    {
        size_t head_len = start_marker - zstr_data(&existing);
        zstr_cat_len(&final_out, zstr_data(&existing), head_len);
        zstr_cat(&final_out, "\n"); zstr_cat(&final_out, zstr_cstr(&generated)); zstr_cat(&final_out, "\n");
        zstr_cat(&final_out, end_marker + strlen(MARKER_END)); 
    } 
    else if (loaded) 
    {
        zstr_cat(&final_out, zstr_cstr(&existing));
        if (!zstr_ends_with(&existing, "\n")) 
        {
            zstr_push(&final_out, '\n');
        }
        zstr_cat(&final_out, "\n"); 
        zstr_cat(&final_out, zstr_cstr(&generated)); 
        zstr_cat(&final_out, "\n");
    } 
    else 
    {
        zstr_cat(&final_out, "# API Reference\n\n"); 
        zstr_cat(&final_out, zstr_cstr(&generated));
    }

    zfile_save_atomic(out_path, zstr_cstr(&final_out), zstr_len(&final_out));
    zstr_free(&existing); zstr_free(&final_out); 
    zstr_free(&doc_content);
    zstr_free(&toc_content); 
    zstr_free(&issues_content); 
    zstr_free(&pending_desc);
    zstr_free(&current_columns); 
    zstr_free(&generated);
    return 0;
}
