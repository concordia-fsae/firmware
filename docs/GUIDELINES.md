# Guidelines

_Internal file templates and section templates located in `./shared/templates/`_

## Programming Guidelines

- Use good **modular design**. Think carefully about the program structure, functions, and data structures before starting to write the code
- Use proper **error detection** and handling. Always check return values from functions and handle errors appropriately. `Error_Handler()` is not proper error handling for production level code (typically).
- If the component uses dynamic memory allocation (typically, we do not support dynamic memory allocation in this framework) you must have code somewhere in the program to release memory.
- Use **descriptive names** for functions and important variables. GetRadius() should be used instead of foo() for a function that returns the radius of a circle.
- **Define constants** and use them, especially for configuration values. If you have a buffer and decide it should be 100 units long, then:

    **Compliant**
    ```
    #define BUFFER_SIZE 50
    ...
    uint8_t buf[BUFFER_SIZE];
    ```

    **Non Compliant**
    ```
    uint8_t buf[50];
    ```

- Keep each variable to the **smallest scope possible**. Do not have a variable global to a file if it is only used from one function. Avoid using global variables
- Use **standard types** such as `uint8_t`, `int32_t`, etc... Do not use `int` or `unsigned int`.
- Application level code should not contain system level details. System level code should not contain firmware level details. Etc... Utilize a **top-down design**
- **Line length** should be kept relative. While we believe in monitors with unlimited width, do not have a line 200 characters long. A good rule of thumd is roughly 100 characters long, or just a bit biggetr than a section heading.
- **Documment your code**, if you are doing something ambiguous, something that relates to hardware details, etc... detail the execution flow of your program. If you know what it does great, but once you are no longer on the team your knowledge will not help the next person unless you look forward to babysitting (for **free**)
    - Every file, function, etc... should be documented in line with the **Documentation Guidelines**
- Only **single assignment per line**, if assigning multiple items do it over multiple lines

    **Compliant**
    ```
    uint32_t a, b, c;
    a = 0;
    b = a;
    c = a;
    ```

    **Non Compliant**
    ```
    uint32_t a, b, c;
    a = b = c = 0;
    ```

## Source and Header Files Style Guidelines

- Use proper **formatting**. Formatting details can be found in `.clang-format`

### File Sections

- Each file is broken into sections such as Includes, Defines, etc...
- An empty line is to follow the section header and two lines are to come before a section header
    
    **Compliant Code**
    ```
    /******************************************************************************
     *                              D E F I N E S
     ******************************************************************************/

    #define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES    2U
    #define ADC_CALIBRATION_TIMEOUT                    10U


    /******************************************************************************
     *                           P U B L I C  V A R S
     ******************************************************************************/
    ```
    **Non-Compliant Code**

    ```
    /******************************************************************************
     *                              D E F I N E S
     ******************************************************************************/
    #define ADC_PRECALIBRATION_DELAY_ADCCLOCKCYCLES    2U
    #define ADC_CALIBRATION_TIMEOUT                    10U

    /******************************************************************************
     *                           P U B L I C  V A R S
     ******************************************************************************/
    ```

### Pre-Processor

- Macros

    - Macros are to be capitalized
        
        **Compliant** `#define NEGATE(x) -(x)`

        **Non Compliant** `#define negate(x) -(x)`

    - Reference to macro variables are to be in brackets
        
        **Compliant** `#define NEGATE(x) -(x)`

        **Non Compliant** `#define NEGATE(x) -x`

- Defines

    - Define names are to be capitalized and seperated by an under-score: `CAPITALIZED_AND_SCORED`
    - Defines must be given **meaningful names**. `#define TOTAL_MPRL_SENSORS 8` instead of `#define N_SENS 8`
    - **Do not use magic numbers**
        
        **Compliant**
        ```
        #define USE_THIS_THING 0
        #define USE_THAT_THING 1
        #define CONFIG_SETTING USE_THIS_THING
        ```
        **Non Compliant**
        ```
        #define CONFIG_SETTING 0
        ```

- Header files

    - Contents should be protected from multiple inclusion with `#pragma once`

        **Compliant**
        ```
        /* In showcase.h */
        #pragma once

        /* header contents */
        uint32_t func_showcase();
        ```

        **Non Compliant**
        ```
        /* In showcase.h */

        /* header contents */
        uint32_t func_showcase();
        ```


### Naming Conventions

- Each name can be broken into its MODULE, LIBRARY, and Specifics
    - **Optional** MODULE: To be capitalized
    - **Optional** LIBRARY: To be capitalized
    - Specifics: Case dependant
1. **Files**
    - To be named with the module and library. Such as `HW_ADC.*` for the firmware of the ADC
2. **Functions**
    - Names are to be given by `MODULE_LIBRARY_TaskSpecifics`. Such as `HW_I2C_MasterTransmit` to transmit data as a master. `TaskSpecifics` to be in PascalCase
3. **Types**
    1. Structs
        - Follow the `MODULE_LIBRARY_Specifics` standard and be appended with `_S`. `Specifics` to be in PascalCase
        
            **Compliant** `SYS_IO_S`

            **Non Compliant** `Io_s`
        - Struct members should be miniscule, seperated by under-scores, and meaningful
            
            **Compliant**
            ```
            typedef struct 
            {
                uint32_t pcb_temp;
                ...
            } HW_EnvValues_S;
            ```

            **Non Compliant**
            ```
            typedef struct 
            {
                uint32_t a;
                ...
            } env_s;
            ```
    2. Enums
        - Follow the `MODULE_LIBRARY_Specifics` standard and be appended with `_E`. `Specifics` to be in PascalCase
        
            **Compliant** `ADC_Channels_E`

            **Non Compliant** `AdcChannels`
        - Members must be capitalized and seperated by underscore
            
            **Compliant**
            ```
            typedef enum
            {
                ADC_CHANNEL_CURRENT_SENSE = 0U,
                ...
            } ADC_Channels_E;
            ```

            **Non Compliant**
            ```
            typedef enum
            {
                currentSense = 0,
                ...
            } ADC_Channels_E;
            ```
        - Indexes are to be started with `= 0U` or `= 0x00`

4. **Variables**
    1. Generic
        - Globals
            - Must be `CAPITALIZED_AND_SCORED`
            - Must be given meaningful name based on `MODULE_LIBRARY_SPECIFICS`
        - Local to module
            - Must be given meaningful name based on `LIBRARY_specifics`. `LIBRARY` to be capitalied and `specifics` to be miniscule
            - Library suffix is optional
        - Local to function
            - Must be `miniscule_and_scored`
            - Must be given meaningful name
        - Intermediates
            - Intermediates should be given a meaningful name in `miniscule_and_scored`. They should be used or discarded

                **Compliant**
                ```
                #define EINVAL 1

                int32_t func_showcase(uint32_t param)
                {
                    int32_t error;

                    if (param < 32U) {
                        error = 0;
                    } else {
                        error = -EINVAL;
                    }

                    return error;
                }

                void main(uint32_t index)
                {
                    int32_t error;
                    uint32_t test;
                    uint32_t array_showcase[32];

                    error = func_showcase(index);
                    func_showcase(index); // Also OK

                    if (error == 0) {
                            test = array_showcase[index];
                    }
                }
                ```
                **Non Compliant**
                ```
                #define EINVAL                22

                int32_t func_showcase(uint32_t param)
                {
                    int32_t error;

                    if (param < 32U) {
                        error = 0;
                    } else {
                        error = -EINVAL;
                    }

                    return error;
                }

                void main(uint32_t index)
                {
                    int32_t error;
                    uint32_t test;
                    uint32_t array_showcase[32];

                    error = func_showcase(index);

                    test = array_showcase[index];
                }
                ```

    2. Structs
        - Must be named with camelCase and appended with `_S`

            **Compliant** `paddlesRaw_S`

            **Non Compliant** `paddles`
    3. Enums
        - Must be named with camelCase and appended with `_E`

            **Compliant** `currentState_E`

            **Non Compliant** `state`

## Documentation Guidelines

_To be used with doxygen-style comments_
    
- Some detailed rules are listed below to illustrate the comments format for each function:
- The comments block shall start with /** (slash-asterisk-asterisk) in a single line.
- The comments block shall end with */ (space-asterisk-slash) in a single line.
- Other than the first line and the last line, every line inside the comments block shall start with * (space-asterisk). It also applies to the line which is used to separate different paragraphs. We’ll call it a blank line for simplicity.

1. **Files**
    - Each file must contain a header which details the file name, author, version, date and a brief description
    
        **Compliant**
        ```
        /* in header.h */
        /**
        * @file header.h
        * @brief  Details the configuration of this implementation
        * @author Joshua Lafleur (josh.lafleur@outlook.com)
        * @version 0.1
        * @date 2022-08-27
        */
        ```

        **Non Compliant**
        ```
        /* in header.h */
        // This code houses the configuration
        ```
2. **Functions**
    - For each function, following information shall be documented: brief description, detailed description, parameters description, pre-conditions, post-conditions, return value description, and comments explaining the actual return values. We’ll call each block of information a paragraph for simplicity. A paragraph may be removed from the list if it is not applicable for that function.
    - Each line shall only contain the description for one parameter, or one pre-condition, or one post-condition, or one actual return value. We’ll call each of these an element for simplicity.
    - A blank line shall separate different paragraphs. Inside each paragraph, a blank line is not required to separate each element.
    - The brief description of the function shall be documented with the format @brief <brief description>.
    - No specific format is required for the detailed description of the function.
    - The description of the function parameter shall be documented with the format @param <parameter name> <parameter description>.
    - The pre-condition of the function shall be documented with the format @pre <pre-condition description>.
    - The post-condition of the function shall be documented with the format @post <post-condition description>.
    - The brief description of the function return value shall be documented with the format @return <brief description of return value>.
    - A void-returning function shall be documented with the format @return None.
    - The comments explaining the actual return values shall be documented with the format @retval <return value> <return value explanation>.
    - If the description of one element needs to span multiple lines, each line shall be aligned to the start of the description in the first line for that element.
    - The comments block shall appear immediately before the function definition/declaration in the C source file or header file.

        **Compliant**
        ```
        /**
         * @brief Brief description of the function.
         *
         * Detailed description of the function. Detailed description of the function. Detailed description of the
         * function. Detailed description of the function.
         * Application Constraints: Detailed description of application constraint.
         *
         * @param param_1 Parameter description for param_1.
         * @param param_2 Parameter description for param_2.
         * @param param_3 Parameter description for param_3. Parameter description for param_3. Parameter description
         *                for param_3. Parameter description for param_3. Parameter description for param_3. Parameter
         *                description for param_3.
         *
         * @pre param_1 != NULL
         * @pre param_2 <= 255U
         *
         * @post retval <= 0
         *
         * @return Brief description of the return value.
         *
         * @retval 0 Success to handle specific case.
         * @retval -EINVAL Fail to handle specific case because the argument is invalid.
         * @retval -EBUSY Fail to handle specific case because the target is busy.
         *
         */
        int32_t func_showcase(uint32_t *param_1, uint32_t param_2, uint32_t param_3);
        ```

        **Non Compliant**
        ```
        /* Brief description of the function.
        Detailed description of the function. Detailed description of the function. Detailed description of the
        function. Detailed description of the function.

        @param param_1 Parameter description for param_1. @param param_2 Parameter description for param_2.
        @param param_3 Parameter description for param_3. Parameter description for param_3. Parameter description
        for param_3. Parameter description for param_3. Parameter description for param_3. Parameter
        description for param_3.

        pre-conditions: param_1 != NULL, param_2 <= 255U
        post-conditions: retval <= 0

        Brief description of the return value. */
        int32_t func_showcase(uint32_t *param_1, uint32_t param_2, uint32_t param_3);
        ```

3. **Types**
    - Types are to be documented on the previous line of the definition and be enclosed in a doxygen-style block
        
        **Compliant** 
        ```
        /**< @brief Documentation of the type */
        typedef uint32_t Color;
        ```

        **Non Compliant** `typedef uint32_t Color; // Documenting`
4. **Variables**
    - Types are to be documented on the same line as the definition and be enclosed in a doxygen-style block
        
        **Compliant** `uint32_t addr; /**< @brief Documentation of the type */`

        **Non Compliant** `uint32_t a; // Documenting`

