# Runs one jscpp program and checks its output.
#   EXE       path to jscpp
#   SRC       program to run (.cppsafe)
#   OUT       optional file with the exact expected stdout
#   ERR       optional file with text that must appear in stderr (the run must then fail)
#   ROUNDTRIP if set, compile SRC to bytecode first and run the .jbc file instead
#   WORKDIR   scratch directory for the .jbc file

set(input "${SRC}")
if(ROUNDTRIP)
    get_filename_component(name "${SRC}" NAME_WE)
    set(input "${WORKDIR}/${name}.jbc")
    execute_process(COMMAND "${EXE}" compile "${SRC}" -o "${input}"
                    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "compile failed: ${err}")
    endif()
endif()

execute_process(COMMAND "${EXE}" run "${input}" --stats
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")

if(OUT)
    file(READ "${OUT}" expected)
    string(REPLACE "\r\n" "\n" expected "${expected}")
    if(NOT out STREQUAL expected)
        message(FATAL_ERROR "stdout mismatch\n--- expected ---\n${expected}--- actual ---\n${out}--- stderr ---\n${err}")
    endif()
endif()

if(ERR)
    file(READ "${ERR}" needle)
    string(STRIP "${needle}" needle)
    if(rc EQUAL 0)
        message(FATAL_ERROR "expected the program to fail with: ${needle}")
    endif()
    string(FIND "${err}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "stderr does not contain '${needle}'\n--- stderr ---\n${err}")
    endif()
elseif(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit status ${rc}\n--- stderr ---\n${err}")
endif()
