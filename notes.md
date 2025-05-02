# Resources
## CMAKE / CMOCKA
https://github.com/Dinesh21Kumar/gtest-example/blob/master/CMakeLists.txt


# TODOS
- define syntax
- define features
- define integration tests (code samples)
- refine repl
- add generational gc

# Features
## values
- integers
- floats (or doubles)
- booleens
- strings / template strings (string interpolation)
- arrays (also associative)
- objects
- getter setter as Datatype?
- enums
- callable
- null
- void?
- core library functions callable directly on values
- byte (bytearray? / binary?)

## variables
- type hints(optional)
- cloning
- dynamic typing
- explicit declaraction? (optional declaration by assignment - :=)

## Operators
- arithmetic
- increment / decrement
- asignment
- bitwise
- comparesion
- execution? (eg. `ls -la`)
- logical
- string concatination
- type 

## constants
- magic constants
  - file
  - line
  - dir
  - function
  - class
  - trait
  - method
  - function
  - namespace
## namespaces
## exceptions
## control flow
- for
- foreach (through interface)
- while
- if / elseif / else
- do-while?
- break / continue
- switch
- match
- return 
- goto?

## comments
- singleline
- multiline

## imports
- populate to (inherit) scope of the line where include occurs
- included files can return values (include can be assigned to variable)
-

## functions
- higer order functions
- assignable to vars (expressions)
- anonymous functions
- lambdas
- callable from string / array?
- typehints for parameters
- default call by value (shallow) optional call by reference 

## classes
- generics
- Attributes
- assignable to vars (expressions)
- nested classes
- traits
- interfaces
- instance of query
- inheritence
- this / super access
- abstract classes
- constructors
  - php like property promotion
- private classes
- properties
    - visibility (public / protected / private / internal?)
      - with default (if none is given public is assumed)
    - fields
      - dynamic access
    - methods
      - dynamic access
    - property overwrites
    - statics
    - typehints for properties
- cloning
- access via dot '.' (not with arrow '->')
- constants
- accessor to get class name
- magic functions
  - construct?
  - destruct?
  - call
  - invoke
  - callStatic
  - get
  - set
  - isset
  - unset
  - toString
  - compare

## reflection
- classes
  - with all features (properties / traits / superclass / interfaces etc.)
- functions
- comments

## misc
- php calls?