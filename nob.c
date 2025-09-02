#define NOB_IMPLEMENTATION
#include "nob.h"
#include "nob_config.h"

// TODO use procs for parallel builds
Nob_Procs Procs;

const char *tests[] = {
    TEST_DIR "/test_queue_basic.c", //
    TEST_DIR "/test_actor_basic.c", //
};

const char *examples[] = {
    EXAMPLE_DIR "/example_thermostat.c", //
};

static void append_compiler(Nob_Cmd *cmd)
{
    nob_cc(cmd);
    nob_cc_flags(cmd);

    // TODO Get the platform specific flags
    nob_cmd_append(cmd, "-Wpedantic", "-Os", "-Iinclude");
}

static bool build_exe(const char *src, const char *out, const char *dep_paths[], size_t dep_paths_count)
{
    if (dep_paths_count > 0)
    {
        if (!nob_needs_rebuild(out, dep_paths, dep_paths_count))
        {
            nob_log(NOB_INFO, "up to date: %s", out);
            return true;
        }
    }
    else
    {
        if (!nob_needs_rebuild1(out, src))
        {
            nob_log(NOB_INFO, "up to date: %s", out);
            return true;
        }
    }

    Nob_Cmd cmd = {0};
    append_compiler(&cmd);
    nob_cmd_append(&cmd, "-lrt", "-lpthread");
    nob_cmd_append(&cmd, src, BUILD_DIR "/courier.o", "-o", out); // TODO abstract courier.o dependency

    bool ret = nob_cmd_run(&cmd);
    nob_cmd_free(cmd);
    return ret;
}

static bool build_obj(const char *output_path, const char **input_paths, size_t input_paths_count)
{
    if (!nob_needs_rebuild(output_path, input_paths, input_paths_count))
    {
        nob_log(NOB_INFO, "up to date: %s", output_path);
        return true;
    }

    Nob_Cmd cmd = {0};
    append_compiler(&cmd);
    nob_cmd_append(&cmd, "-lrt", "-lpthread");

    for (size_t i = 0; i < input_paths_count; i++)
    {
        if (nob_sv_end_with(nob_sv_from_cstr(input_paths[i]), ".c"))
        {
            nob_cmd_append(&cmd, input_paths[i]);
        }
    }
    nob_cmd_append(&cmd, "-o", output_path, "-c");

    bool ret = nob_cmd_run(&cmd);
    nob_cmd_free(cmd);
    return ret;
}

static bool build_lib(const char *src, const char *out)
{
    if (!nob_needs_rebuild1(out, src))
    {
        nob_log(NOB_INFO, "up to date: %s", out);
        return true;
    }

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "ar", "rcs", out, src);

    bool ret = nob_cmd_run(&cmd);
    nob_cmd_free(cmd);
    return ret;
}

// TODO courier platform

void nob_sb_find_and_replace(Nob_String_Builder *sb, const char *find, const char *replace)
{
    const size_t find_len = strlen(find);
    const size_t replace_len = strlen(replace);

    if (find_len == 0) // cannot find an empty string
        return;

    char *cmp_head = sb->items;   // Pointer to the current position in sb for comparing with 'find'
    size_t remaining = sb->count; // Remaining bytes to check from cmp_head

    while (remaining >= find_len)
    {
        // Found an occurrence of 'find'
        // |------------------sb----------------------|
        // ^<--prefix-->^<--find-->---remaining------->
        // |            |
        // sb->items    cmp_head
        if (memcmp(cmp_head, find, find_len) == 0)
        {
            const size_t new_size = sb->count - find_len + replace_len; // New size after replacing 'find' with 'replace'
            const size_t prefix_len = cmp_head - sb->items;             // Size of everything before 'find'
            const size_t suffix_len = remaining - find_len;             // Size of everything after 'find'

            // Make sure sb has enough capacity to hold the new size
            if (new_size > sb->capacity)
            {
                nob_da_reserve(sb, new_size);
            }

            // "Remove" 'find' and suffix from sb. They are still in allocated memory though
            sb->count -= (find_len + suffix_len);

            // If replace_len is larger than find_len we will overwrite part of the suffix if we add it directly:
            // Moving the suffix where it should be now avoids overlap when copying 'replace'
            char *const suffix_dest = sb->items + prefix_len + replace_len;
            const char *const current_suffix_start = sb->items + prefix_len + find_len;
            memmove(suffix_dest, current_suffix_start, suffix_len);

            // Place 'replace' where 'find' was avoiding overlap with the suffix
            char *const replace_dest = sb->items + prefix_len;
            memcpy(replace_dest, replace, replace_len);

            // Update the size of sb
            sb->count += new_size;

            // Ensure sb is null-terminated
            // If find_len >= replace_len, we have overwritten part of the suffix on top of the old null terminator
            nob_sb_append_null(sb);

            // Place the comparison head after the newly appended 'replace' for the next iteration
            cmp_head = replace_dest + replace_len;

            // Update remaining bytes to check
            remaining += (replace_len - find_len);

            // We will modify the sb until no more occurrences of 'find' are found
        }
        else
        {
            cmp_head++;
            remaining--;
        }
    }
}

static bool build_tests(void)
{
    bool result = true;
    Nob_String_Builder sb_out = {0};

    for (size_t i = 0; (i < NOB_ARRAY_LEN(tests) && result); i++)
    {
        nob_sb_append_cstr(&sb_out, tests[i]);

        nob_sb_find_and_replace(&sb_out, TEST_DIR, BUILD_DIR);
        nob_sb_find_and_replace(&sb_out, ".c", "");

        const char *source_paths[] = {BUILD_DIR "/courier.o"};

        result = build_exe(tests[i], nob_temp_sv_to_cstr(nob_sb_to_sv(sb_out)), source_paths, NOB_ARRAY_LEN(source_paths));

        sb_out.count = 0;
    }

    nob_sb_free(sb_out);
    return result;
}

static bool run_tests(void)
{
    Nob_Cmd cmd = {0};
    Nob_String_Builder sb_out = {0};
    bool result = true;

    for (size_t i = 0; i < NOB_ARRAY_LEN(tests); i++)
    {
        nob_sb_append_cstr(&sb_out, tests[i]);

        nob_sb_find_and_replace(&sb_out, TEST_DIR, BUILD_DIR);
        nob_sb_find_and_replace(&sb_out, ".c", "");

        nob_cmd_append(&cmd, nob_temp_sv_to_cstr(nob_sb_to_sv(sb_out)));

        if (!nob_cmd_run(&cmd))
        {
            nob_return_defer(false);
        }

        sb_out.count = 0;
    }

defer:
    nob_sb_free(sb_out);
    nob_cmd_free(cmd);
    return result;
}

static bool build_examples(void)
{
    bool ret = true;
    Nob_String_Builder sb_out = {0};

    for (size_t i = 0; (i < NOB_ARRAY_LEN(examples) && ret); i++)
    {
        nob_sb_append_cstr(&sb_out, examples[i]);

        nob_sb_find_and_replace(&sb_out, EXAMPLE_DIR, BUILD_DIR);
        nob_sb_find_and_replace(&sb_out, ".c", "");

        // ret = build_exe(examples[i], nob_temp_sv_to_cstr(nob_sb_to_sv(sb_out)));
        const char *source_paths[] = {BUILD_DIR "/courier.o"};

        ret = build_exe(examples[i], nob_temp_sv_to_cstr(nob_sb_to_sv(sb_out)), source_paths, NOB_ARRAY_LEN(source_paths));

        sb_out.count = 0;
    }

    nob_sb_free(sb_out);
    return ret;
}

static void handle_clean(void)
{
    Nob_File_Paths files_to_clean = {0};
    Nob_String_Builder sb = {0};

    if (!nob_read_entire_dir(BUILD_DIR, &files_to_clean))
    {
        goto defer;
    }

    for (size_t file_idx = 0; file_idx < files_to_clean.count; ++file_idx)
    {
        if (strcmp(files_to_clean.items[file_idx], ".") == 0)
        {
            continue;
        }

        if (strcmp(files_to_clean.items[file_idx], "..") == 0)
        {
            continue;
        }

        nob_sb_append_cstr(&sb, BUILD_DIR "/");
        nob_sb_append_cstr(&sb, files_to_clean.items[file_idx]);
        nob_sb_append_null(&sb);

        nob_delete_file(nob_sb_to_sv(sb).data);
        sb.count = 0;
    }

defer:
    nob_sb_free(sb);
    nob_da_free(files_to_clean);
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool need_run_tests = false;

    if (argc >= 2)
    {
        const size_t n = argc;

        for (size_t i = 0; i < n; i++)
        {
            const char *a = nob_shift(argv, argc);
            Nob_String_View sv = nob_sv_from_cstr(a);

            // Clean
            if (nob_sv_eq(sv, nob_sv_from_cstr("clean")))
            {
                handle_clean();
                return 0;
            }

            // Rebuild
            if (nob_sv_eq(sv, nob_sv_from_cstr("rebuild")))
            {
                handle_clean();
            }

            // Tests
            if (nob_sv_eq(sv, nob_sv_from_cstr("test")))
            {
                need_run_tests = true;
            }
        }
    }

    // Create build dir if needed
    if (!nob_mkdir_if_not_exists(BUILD_DIR))
        return 1;

    // Build Courier object file
    const char *courier_srcs[] = {SRC "/courier.c", INC "/courier.h"};
    if (build_obj(BUILD_DIR "/courier.o", courier_srcs, NOB_ARRAY_LEN(courier_srcs)))
    {
        // Build Courier static library
        if (!build_lib(BUILD_DIR "/courier.o", BUILD_DIR "/courier.a"))
            return 1;

        // Build examples
        if (!build_examples())
            return 1;

        if (need_run_tests)
        {
            // Build tests
            if (build_tests())
            {
                // Run tests
                if (!run_tests())
                    return 1;
            }
        }

        return 0;
    }

    return 1;
}