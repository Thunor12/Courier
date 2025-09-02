#define NOB_IMPLEMENTATION
#include "nob.h/nob.h"
#include "nob_config.h"

typedef struct dep
{
    const char *src;
    const char *out;
} st_dep;

// TODO use procs for parallel builds
Nob_Procs Procs;

st_dep tests[] = {
    {TEST_DIR "/test_queue_basic.c", BUILD_DIR "/test_queue_basic"}, //
    {TEST_DIR "/test_actor_basic.c", BUILD_DIR "/test_actor_basic"}, //
};

st_dep examples[] = {
    {EXAMPLE_DIR "/example_thermostat.c", BUILD_DIR "/example_thermostat"}, //
};

static void append_compiler(Nob_Cmd *cmd)
{
    nob_cc(cmd);
    nob_cc_flags(cmd);

    // TODO Get the platform specific flags
    nob_cmd_append(cmd, "-Wpedantic", "-Os", "-Iinclude");
}

static bool build_exe(const char *src, const char *out)
{
    if (!nob_needs_rebuild1(out, src))
    {
        nob_log(NOB_INFO, "up to date: %s", out);
        return true;
    }

    Nob_Cmd cmd = {0};
    append_compiler(&cmd);
    nob_cmd_append(&cmd, "-lrt", "-lpthread");
    nob_cmd_append(&cmd, src, BUILD_DIR "/courier.o", "-o", out); // TODO abstract courier.o dependency
    return nob_cmd_run(&cmd);
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

    return nob_cmd_run(&cmd);
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
    return nob_cmd_run(&cmd);
}

// TODO courier platform

static bool build_tests(void)
{
    bool ret = true;
    for (size_t i = 0; (i < NOB_ARRAY_LEN(tests) && ret); i++)
    {
        ret = build_exe(tests[i].src, tests[i].out);
    }

    return ret;
}

static bool run_tests(void)
{
    Nob_Cmd cmd = {0};
    for (size_t i = 0; i < NOB_ARRAY_LEN(tests); i++)
    {
        nob_cmd_append(&cmd, tests[i].out);

        if (!nob_cmd_run(&cmd))
        {
            return false;
        }
    }

    return true;
}

static bool build_examples(void)
{
    bool ret = true;
    for (size_t i = 0; (i < NOB_ARRAY_LEN(examples) && ret); i++)
    {
        ret = build_exe(examples[i].src, examples[i].out);
    }

    return ret;
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
                nob_log(NOB_INFO, "cleaning ...");
                Nob_File_Paths files_to_clean = {0};
                nob_read_entire_dir(BUILD_DIR, &files_to_clean);

                for (size_t j = 0; j < files_to_clean.count; ++j)
                {
                    if (strcmp(files_to_clean.items[j], ".") == 0)
                    {
                        continue;
                    }

                    if (strcmp(files_to_clean.items[j], "..") == 0)
                    {
                        continue;
                    }

                    Nob_String_Builder sb = {0};
                    nob_sb_append_cstr(&sb, BUILD_DIR "/");
                    nob_sb_append_cstr(&sb, files_to_clean.items[j]);
                    nob_sb_append_null(&sb);

                    nob_delete_file(nob_sb_to_sv(sb).data);
                    nob_sb_free(sb);
                }

                return 0;
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