# CMake function to handle binary embedding using NASM
function(add_incbin_target target_name binary_file symbol_name)
    # Generate NASM assembly file
    set(ASM_FILE "${CMAKE_CURRENT_BINARY_DIR}/${symbol_name}.asm")

    # Get absolute path for incbin
    get_filename_component(ABS_BINARY_PATH "${binary_file}" ABSOLUTE)

    # Generate the assembly file at configure time
    file(WRITE ${ASM_FILE}
"section .data
global ${symbol_name}
global ${symbol_name}_end
${symbol_name}:
    incbin \"${ABS_BINARY_PATH}\"
${symbol_name}_end:
")

    # Create object library with the assembly file
    add_library(${target_name} OBJECT ${ASM_FILE})

    # Set NASM object format for Windows
    if(WIN32)
        set_property(TARGET ${target_name} PROPERTY ASM_NASM_OBJECT_FORMAT win64)
    endif()
endfunction()

# Higher-level function to add multiple binary resources to a target
function(target_binary_resources target_name visibility)
    set(remaining_args ${ARGN})

    # Process arguments in pairs (symbol_name file_path)
    while(remaining_args)
        list(LENGTH remaining_args remaining_count)
        if(remaining_count LESS 2)
            message(FATAL_ERROR "target_binary_resources: Arguments must be in pairs of symbol_name file_path")
        endif()

        # Get the symbol name and file path
        list(GET remaining_args 0 symbol_name)
        list(GET remaining_args 1 file_path)
        list(REMOVE_AT remaining_args 0 1)

        # Create individual incbin target
        set(resource_target_name "${target_name}_${symbol_name}_obj")
        add_incbin_target(${resource_target_name} "${file_path}" "${symbol_name}")

        # Link it to the main target
        target_link_libraries(${target_name} ${visibility} ${resource_target_name})
    endwhile()
endfunction()
