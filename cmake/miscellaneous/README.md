## Introduction
This is a set of additional helpers that can be use to configure a **CMake**
based project.

## Set-up
In order to allow the use of this helpers, the CMakeCommon module **must** first
be enabled from the main project wishing to use them.

Please, refer to the [CMakeCommon](../README.md#usage)'s help for more
information about how to enable this module.

## Usage
To use on the available feature, use the **CMake**'s *include(...)* directive
with the name of the feature.

## Available features
### EnhancedList
Add helpers to manipulate cmake lists.

---
#### list_join
```cmake
 list_join(
   <list_name>    # Name of the list
   <separator>    # String to use as separator for each list items
   <destination>  # Name of the variable that will host the resulting string
 )
```
This function joins all items of a list into a string. Each items stored in
the destination string will be separated with a given string separator.

### FileEdit
Implementation of tools to edit text files

---
#### file_create_unique
```cmake
 file_create_unique(
   <var_name>              # Name of variable where to store file's name
   [PREFIX <file_prefix>]  # Prefix to use for file's name
   [SUFFIX <file_suffix>]  # Suffix to add to file's name
   [CONTENT <Content>]     # Optional content to write in created file
   [PATH <path>]           # Path to where the file is to be created
 )
```
This function creates a file using a unique name. If the content is provided,
it is used to fill the file.

---
#### file_load
```cmake
 file_load(
   FILE       <file_path>  # Path to the file to load
   OUTPUT     <var_name>   # Name of the parent's variable where to store
                           # file's content
   MERGE_SPLIT             # Ask to reassemble split lines
 )
```
This function loads a file's content and store it in a destination list
variable owned by the caller (i.e. parent)

> **Note**
>
> The MERGE_SPLIT option is to force file's split lines to be reassembled
> into a single line.
> A split line is a line ending with the character '\' followed with
> a EOL character (i.e. '\n'  special character)
>
> Example:
> ```cmake
>  file_load(
>     FILE        my_file
>     OUTPUT      lines
>     MERGE_SPLIT
>  )
>  message("'${lines}'")
> ```
>
> With the following content for 'my_file':
> ```text
> Hello World
> I \
> love \
> football
> ```
> The displayed message will be a list with **2** elements:
> ```text
> 'Hello World;I love football'
> ```

---
#### file_edit
```cmake
 file_edit(
   FILE <list_of_files>  # of files to edit
   RULE                  # Edit rule to apply to given files
     <line_filter>       #   Filter for lines the rule has to be applied to
     <regex_find>        #   Regular expression to line's part to be edited
     <regex_replace>     #   Regular expression for modification to apply
   MERGE_SPLIT           # Ask to reassemble split lines
 )
```
This function is used to edit the input files using a given set of rules.

> **Note**
>
> To provide more than one RULE, just add a new RULE line in your call.
>
> Example:
> ```cmake
>  file_edit(
>    FILE my_file
>    RULE ""         "o"  "O"
>    RULE "fOOtball" "ll" "L2"
>  )
> ```
> With the following content for 'my_file':
> ```text
> Hello World
> I love football
> ```
> The resulting file's content will be:
> ```
> HellO WOrld
> I lOve fOOtbaL2
> ```

### GetGitRevisionDescription
**TODO**
### HelpersArguments
Defines some helpers to handle function/macro's arguments

---
#### parse_arguments
```cmake
 parse_arguments(
   <PREFIX>  # Prefix that will be added to created variable name
   <OPTIONS> # List of boolean options
   <SINGLES> # List of single value options
   <LISTS>   # List of multiple values options
   <MAPS>    # List of complex values (single and nested multiple) options
   <ARGS...> # Arguments to match against provided options
 )
```
Parses the given arguments and dispatches them among the specified fields.
It adds some enhancements to the legacy *cmake_parse_arguments(...)*:

* The lists will no longer be reset each the list start token is present.
Instead, the new values for a list will be added to the already filled list.
* The maps offer the possibility to accepts lists of lists

All parsed arguments will be stored in their respective created variable
of the form `<PREFIX>_<NAME>` where `<NAME>` will be the name of the associated
option.
> **Note**
>
> All the unparsed arguments will be stored in created variable named
> `<PREFIX>_ARGN`.

### HelpersIdeTarget
Defines some helpers for IDE's Groups management.

---
#### apply_properties
```cmake
 apply_properties(<TYPE> <ENTITY> prop1=value1 [prop2=value2] ...)

 With:
   <TYPE>    Type of the following entity.
             It can be any  from <DIRECTORY|TARGET|SOURCE|TEST|CACHE>
   <ENTITY>  name of the entity (of type <TYPE>) the properties have to be
             applied to
```
This function is a helper to ease the modifications of a entity's properties.

> **Note**
>
> Properties' syntax has to follow rule:
>  `<PROP_NAME>[:+]=<PROP_VALUE>`
>
> With:
> `<PROP_NAME>` is the name of the property
> `<PROP_VALUE>` is the value to apply to the property
>
> The form of the assignment operator indicates is how value has to be assign
> to the property:
> * `=`  set the property with the given value
> * `:=` set the property with the given value only if property is not already
>set
> * `+=` add the given value to the property as a list element
> * `~=` concatenate the given value to the string property

---
#### add_target
```cmake
  add_target(
    <target_name>                  # Name of target to create
    TYPE <name> [<options>]        # Type of the target
    FILES <files>                  # Source files associated to target

    # --- Compilation/link specific directives ---
    [LINKS <links>]                # List of libraries to link the target with
    [DEPENDS <depends>]            # List of target's dependencies
    [DEFINITIONS <defs>]           # List of target's compile definitions
    [INCLUDE_DIRECTORIES <paths>]  # List of target's include search paths

    # --- Configuration specific directives ---
    [PROPERTIES <props>]           # List of target's specific properties

    # --- Test target's specific directives ---
    [TEST_ARGS <args>]             # List target's specific arguments to use
                                   # provide to test on running it
    [TEST_PROPERTIES <props>]      # List of target's properties specific to
                                   # tests

    # --- Installation specific directives ---
    [INSTALL]                      # Specific that target must be added to
                                   # the global install directive
    [INSTALL_PATH <path>]          # Target's specific installation path

    # --- IDE specific directives ---
    [PREFIX_SKIP <reg-ex>]         # Regular expression to apply to files to
                                   # skip root path
    [SOURCE_GROUP_NAME <name>]     # Name of group for identified source files
    [HEADER_GROUP_NAME <name>]     # Name of group for identified header files
    [RESOURCE_GROUP_NAME  <name>]  # Name of group for identified resource_files
    [SOURCE_FILTER <reg-ex>]       # Regular expression to identify source files
    [HEADER_FILTER <reg-ex>]       # Regular expression to identify header files
    [GROUP <name>]                 # Name of IDE's group tree where reference
                                   # target
    [LABEL <name>]                 # Name of the target in the IDE's group tree
    [LAUNCHER <flags>]             # Set-up target's launcher
 )
```
Helper to create a **CMake** target with various grouped settings. If used
generator targets an IDE, the target's files will be grouped into virtual
folders/groups tree.

Here are details about the various available settings:

* `TYPE <name> [<options>]`

    Three `<name>` (i.e. types) are available:
    * **`EXECUTABLE`:** Target is a executable application
    * **`LIBRARY`:** Target is a linkable (static or dynamic) library

    Each of these types may be completed with generic options:
    * **`EXCLUDE_FROM_ALL`:** The target will not be attached to 'all' build
    rule
    * **`RUNTIME_STATIC`:** The target must be linked against the static
    runtime

    Some other specific options are also available:
    * For `EXECUTABLE`:
        * **`TEST`:** The application is to be attached to unit tests stage
            > **Note**
            >
            > The `TEST` option implies, in addition to the target itself,the
            > creation of **CMake**'s test targets (named `<name>` but suffixed
            > with `_test`).

            If the global variable `UNIT_TEST_REPORTS_DIR` is set, then unit
            tests main reports will no longer be stored in the 'gtest_reports'
            sub-directory of the root build directory but in the location path
            specified in the variable.

            If the global flag `ENABLE_SUBTEST_TARGETS` is set to `ON`, then
            additional dedicated build-able targets will be created for each
            sub-tests found in the target's source files.
        * **`WIN32`:** Windows console less application
        * **`MACOSX_BUNDLE`:** MacOSX console less application
    * For `LIBRARY`:
        * **`SHARED`:** Shared library for dynamic linking
        * **`MODULE`:** Module for manual linking
        * **`STATIC`:** Static link library

  > **Note**
  >
  > See help for **CMake**'s functions `add_executable(...)` and
  > `add_library(...)` to get more information about options.

* `FILES <files>`

    This specifies the list of (path) files to attach to the target to create.

  > **Note**
  >
  > The specified files can use absolute or relative paths. But if the used
  > generator targets an IDE, then ensure to set the `PREFIX_SKIP` to right
  > value. Otherwise, the complete file's path will be used to generate a
  > virtual tree.

* `LINKS <links>`

    If the target should link against external libraries, then they must be
    specified here. The links can be:
    * a path to a binary library
    * a **CMake** target name
    * a platform specific link option

  > **Note**
  >
  > See help for **CMake**'s function `target_link_libraries(...)` to get more
  > information.

* `DEPENDS <depends>`

    If the target may depend on other targets, then they must be specified here.

  > **Note**
  >
  > See help for **CMake**'s function `add_dependencies(...)` to get more
  > information.

* `DEFINITIONS <definitions>`

    Set some preprocessor definitions to target. These definitions can be either
    tagged `PUBLIC`, `INTERFACE` or `PRIVATE`.

  > **Note**
  >
  > See help for **CMake**'s function `target_compile_definitions(...)` to get
  > more information.

* `INCLUDE_DIRECTORIES  <path>`

    Set some addition search paths for preprocessor `include<...>` directive.
    These search path can be either tagged `PUBLIC`, `INTERFACE` or `PRIVATE`.

  > **Note**
  >
  > See help for **CMake**'s function `target_include_directories(...)` to get
  > more information.

* `PROPERTIES <properties>`

    Set properties to target. These properties can be either to influence
    **CMake**'s behaviour or for project's internal use.

  > **Note**
  >
  > Properties follow the syntax rule defined in
  > [`apply_properties(...)`](#apply_properties).

* `TEST_ARGS <args>`

    Specify the list of command line arguments to provide to test's target when
    it is executed through **CMake**'s rule.

* `TEST_PROPERTIES <properties>`

    Allows to set test's specific properties such as `WORKING_DIRECTORY` and
    `CONFIGURATIONS`.

  > **Note**
  >
  > Properties follow the syntax rule defined in
  > [`apply_properties(...)`](#apply_properties).

* `INSTALL`

    Specify the target must be added to the list of artefacts to install through
    the **CMake**'s `install` rule.

* `INSTALL_PATH`

    Set a specific installation directory for this target.

* `PREFIX_SKIP <reg-ex>` (defaults to `(\./)?(src|sources)`)

    Set a regular expression pattern to apply to remove leading files path that
    should not appear in the IDE's virtual tree.

* `SOURCE_GROUP_NAME <name>` (defaults to `Source Files`)

    Name of the sub group where the IDE should reference the source files.

* `HEADER_GROUP_NAME <name>` (defaults to `Source Files`)

    Name of the sub group where the IDE should reference the header files.

* `RESOURCE_GROUP_NAME <name>` (defaults to `Resource Files`)

    Name of the sub group where the IDE should reference the resource files.

  > **Note**
  >
  > A resource file is a file that matches neither `SOURCE_FILTER` nor
  > `HEADER_FILTER` filters.

* `SOURCE_FILTER <reg-ex_filter>` (defaults to `\.(c(\+\+|xx|pp|c)?|C|M|mm)`)

    Regular expression to help to identify if a file is a source file.

* `HEADER_FILTER <reg-ex_filter>` (defaults to `\.h(h|m|pp|\+\+)?`)

    Regular expression to help to identify if a file is a header file.

* `GROUP <name>` (defaults `Executable`, `Unit Tests` or `Libraries`)

    Tell the IDE to locate the target under the given groups path.

* `LABEL <name>` (defaults to target's name)

    Set the name the IDE should use to reference the target.

* `LAUNCHER <flags>`

    Set special flags to used IDE to start the target with a special environment
    (`WORKING_DIRECTORY`, `ENVIRONMENT`, ...)

  > **Note**
  >
  > See documentation for [`create_launcher(...)`](https://gitlab.sources.net/externals-cmakemodules/cmake-modules/blob/master/CreateLaunchers.cmake)
  > for more information.

> **Example**
>
> ```cmake
> add_target(myTargetLib
>     TYPE
>         LIBRARY STATIC
>     FILES
>         "src/files/gui/main.cpp"
>         "src/files/gui/dialog.cpp"
>         "src/files/gui/dialog.res"
>         "src/files/tools/convert.cpp"
>         "src/files/gui/dialog.h"
>         "src/files/tools/convert.h"
>     PREFIX_SKIP
>         "src/files"
>     DEFINITIONS INTERFACE
>         MY_DEF1=VAL1
>         MY_DEF2=VAL2
>     DEFINITIONS PRIVATE
>         MY_NO_VAL_DEF
> )
>
> add_target(myTarget
>     TYPE
>         EXECUTABLE
>     FILES
>         "src/files/app/main.cpp"
>         "src/files/app/all.h"
>     PREFIX_SKIP
>         "src/files"
>     LINKS
>         myTargetLib
> )
> ```
>
> will create a target for an executable named *myTarget*. For the IDE, the
> target will be composed of a virtual groups tree that hold the files:
>
> ```text
>  |- Executables
>  |  \- myTargetLib
>  |     \- Source Files
>  |        \- app
>  |           |- main.cpp
>  |           \- all.h
>  |
>  \- Libraries
>     \- myTargetLib
>        |- Source Files
>        |  |- gui
>        |  |  |- main.cpp
>        |  |  \- dialog.cpp
>        |  |  \- dialog.h
>        |  |
>        |  \- tools
>        |     |- convert.cpp
>        |     \- convert.h
>        |
>        \- Resource Files
>           \ - gui
>               \- dialog.res
> ```
>
> **Note**
>
> If the `PREFIX_SKIP` parameter is omitted, then the above result would be:
> ```text
>  |- Executables
>  |  \- myTargetLib
>  |     \- Source Files
>  |        \- files
>  |           \- app
>  |              |- main.cpp
>  |              \- all.h
>  |
>  \- Libraries
>     \- myTargetLib
>        |- Source Files
>        |  \- files
>        |     |- gui
>        |     |  |- main.cpp
>        |     |  \- dialog.cpp
>        |     |  \- dialog.h
>        |     |
>        |     \- tools
>        |        |- convert.cpp
>        |        \- convert.h
>        |
>        \- Resource Files
>           \- files
>              \ - gui
>                  \- dialog.res
> ```

---
#### project_group
```cmake
 project_group(
   <group_tree>
   [<list_of_targets> |
    TARGET <target_name>
    LABEL <target_name_in_group>]
 )
```
Set target(s) as being hold by a specific project group path
A Target associated to a group tree will appear under the associated project
group in the IDE.

> **Example**
>
> ```cmake
> project_group(
>     "tools/commandlines"
>     TARGET "myTarget" LABEL "theTool"
> )
> ```
>
> will create a virtual project group tree to hold the target:
>
> ```text
>  tools              \
>  |                  | -> Created group tree
>  \-  commandlines   /
>      |
>      \-  theTool    | -> Displayed name for the target 'myTarget'
> ```

---
#### file_group
```cmake
 file_group(
   <group_name>            # Name of IDE's target group
   FILES <paths>           # List of files to reference in IDE's group
   [PREFIX_SKIP <reg-ex>]  # Reg-exp to filter files' root path
                           # (defaults to '(\./)?(sources|src)')
 )
```
Associates a list of files to a virtual group tree

> **Example**
>
> ```cmake
> file_group("Sources"
>     PREFIX_SKIP     "\\.\\./src"
>
>     FILES
>         ../src/gui/main.cpp
>         ../src/gui/dialog.cpp
>         ../src/tools/convert.cpp
> )
> ```
>
> will create a virtual group tree to hold the files:
>
> ```text
>  Source
>  |-  gui
>  |   |- main.cpp
>  |   \- dialog.cpp
>  |
>  \-  tools
>      \- convert.cpp
> ```
>
> **Note**
>
> If the `PREFIX_SKIP` parameter is omitted, then the above result would had
> been:
> ```text
>   Source
>   \- ..
>      \- src
>         |- gui
>         |  |- main.cpp
>         |  \- dialog.cpp
>         |
>         \- tools
>            \- convert.cpp
> ```

---
#### enable_coding_style
```cmake
 enable_coding_style(
   [FILES   <list_of_files>]    # List of files to apply coding style check
   [TARGETS <list_of_targets>]  # List of targets whose files have to be checked
 )
```
Proceed to a (Google) coding style check to each specified files or targets'
files each time a modification is triggered.

> **Examples**
>
> 1. No Target but files
>
>     ```cmake
>     enable_coding_style(
>       FILES
>         ../src/gui/main.cpp
>         ../src/gui/dialog.cpp
>         ../src/tools/convert.cpp
>     )
>     ```
>     will add target to check coding style of the specified files.
> 2. Target but no files
>
>     ```cmake
>     enable_coding_style(
>       TARGETS
>         target_1
>         target_2
>     )
>     ```
>     will add target to check coding style on all files associated to the
>     specified targets.
> 3. Target **AND** files
>
>     ```cmake
>     enable_coding_style(
>       TARGETS
>         target_1
>         target_2
>       FILES
>         ../src/gui/main.cpp
>         ../src/gui/dialog.cpp
>         ../src/tools/convert.cpp
>     )
>     ```
>     will add target to check coding style on:
>     * all files associated to the specified targets.
>     * all files specified

---
#### enable_target_linked_artefact_copy
```cmake
 enable_target_linked_artefact_copy(
   <list_of_targets>  # List of targets to enable copy
 )
```
Sets up specified targets to copy all artefacts of their linked modules or
shared libraries to the respective binary directories hosting their artefacts.

This feature can be useful when running, in debug mode, applications that link
against shared libraries provided by other targets.

### MSVCCleanupRuntimeFlags
Helper to remove ALL default MSVC's flags linked to Runtime link

### Manifest
**TODO**

### MultiList
Implementation of a multi list behaviour

The main difference versus a legacy list is that all provided elements will be
aggregated and then insert as a single element which is, at the end, a list

---
#### multi_list
```cmake
 multi_list(
   <action>       # Action to apply to list
   <list_name>    # The name of the multi-list the command has to be applied to
   <command_args> # The arguments associated to the specified command
 )
```
> **Note**
>
> All the legacy list's action are accepted. In addition, the action `APPEND_AT`
> has been created to add new items to an item list present in the multi-list.
>
> The syntax of the `APPEND_AT` is:
> ```cmake
> APPEND_AT <list_name> <item_index> <new_elements> ...
> ```

### SystemTools
Implementation of tools to perform some system actions

---
#### system_run
```cmake
 system_run(
   [EXEC <command> [args]]          # Command line to execute
   [EVAL <command> [<command>...]]  # List of instructions to evaluate
 )
```
This function runs (i.e. executes or evaluates) the given commands
> **Note**
>
> This function is based on [system_execute(...)](#system_execute) and
> [system_eval(...)](#system_eval)

---
#### system_eval
```cmake
 system_eval(
   <command> [<command>...]  # List of instructions to evaluate
 )
```
This function evaluates the given list of **CMake**'s script lines.

---
#### system_execute
```cmake
 system_execute(
   NO_LOG                   # Disable LOG feature. Outputs are not redirected.
   NO_LOG_DUMP              # Disable automatic dump of log in case of error
   LOG <file_path>          # Path to the file that will host the execution log
   LOG_VAR <name>           # Name of variable that will receive execution log
   DEBUG                    # Enable debug (i.e. verbose) mode
   WORKING_DIR <dir_path>   # Working directory execution should be performed
   COMMAND <command_path>   # Path to the command to execute
   ARGS <arg1> [<arg2 ...]  # List of arguments to associate to command
 )
```
This function executes the given command with the specified arguments.

> **Note**
>
> 1.  If the executed command ends with a result that differs from 0, then
> this command does not return but prints out an error message.
> 2.  `LOG_VAR` cannot stand together with `LOG`, `NO_LOG` and `NO_LOG_DUMP`
> directives.


### Version
**TODO**
### build_file_list

---
#### build_glob_pattern
```cmake
  build_glob_pattern(
    <path>        # Path patterns are relative to
    <patterns>    # Files glob pattern to use
    <result_var>  # Destination variable's name that will host resulting lists
  )
```
Build a list of glob patterns using given reference path and file patterns

> **Example**
>
> ```cmake
>  build_glob_pattern(
>      "../src/"
>      "*.cpp;*.c"
>      glob_pattern
>  )
> message("${glob_pattern}")
> ```
>
>  will display the list :
>
> ```text
>  "../src/*.cpp;../src/*.c"
> ```

---
#### get_matching_files
```cmake
 get_matching_files(
   <relative_path>  # Relative path where to find files
   <pattern_list>   # List of patterns files must match
   <result>         # Destination variable's name where to store resulting list
 )
```
Build a list of all files matching given patterns that are contained in the
given relative directory path.

In addition to given glob patterns, only the source files written for the
targeted platform will be retained. In other words, all files not located in
```cmake
'${relative_path}/*' and '${relative_path}/os_specific/${CMAKE_SYSTEM_NAME}/*'
```
will be excluded.



> **Example**
>
> with the following tree:
> ```text
>   src
>   |- gui
>   |  |- main.c
>   |  \- dialog.cpp
>   |
>   |- tools
>   |  \- convert.cpp
>   |
>   \- os_specific
>      |- Windows
>      |  \- windows_src.cpp
>      |- Darwin
>         \- darwin_src.cpp
> ```
> calling on OsX:
> ```cmake
> get_matching_files("src/" "*.cpp;*.c" result)
> message("${result}")
> ```
> will produce the displayed message:
> ```test
> "src/gui/main.c;src/gui/dialog.cpp;src/tools/convert.cpp;src/os_specific/Darwin/darwin_src.cpp"
> ```

### helpers_target
Define some helpers for Target management

---
#### add_target_definitions
```cmake
 add_target_definitions(
   TARGET <targets_name>      # The name of the targets to add definitions to
   DEFINITIONS <definitions>  # The definitions to add to targets
 )
```
Add new definitions to specified target
> **Example**
> ```cmake
>  add_target_definitions(
>      TARGET
>         myTarget1
>         myTarget2
>
>     DEFINITIONS
>         MY_DEF1=VAL1
>         MY_DEF2=VAL2
>         MY_DEF
> )
> ```

---
#### add_target_link_flags
```cmake
 add_target_link_flags(TARGET <targets_name> FLAGS <new_definitions_to_add>
   TARGET <targets_name>      # The name of the targets to add link flags
   [MINSIZEREL|RELEASE|RELWITHDEBINFO|DEBUG]
       <flags>                # The link flags for target's specific link mode
)
```
Add new linker's flags to specified target

> **Example**
> ```cmake
> add_target_link_flags(
>     TARGET
>         myTarget1
>         myTarget2
>
>     DEBUG
>         debug_flag_1
>         debug_flag_2
>
>     RELEASE
>         release_flag_1
>         release_flag_2
> )
> ```