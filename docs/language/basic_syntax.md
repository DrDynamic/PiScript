# Basic Syntax

The Syntax of WIP is familiar to C-like languages, similar to Javascript with a class system similar to PHP.

I hope you can feel at home right away when you know javascript. 

## Comments
Single line comments start with `//` and end with a linebreak:
```wip
// I am a single line comment 
```

Block comments Start with `/*` and end with `*/`. Linebreaks an nested comments are allowed in there.

```wip
/* I Am
a block comment.
// This is allowed in here!
   /* This is allowed too! */
Still a comment...
*/
```

## Identifiers
Identifiers can be written like in the most other Languages:
```wip
lowercase
cammelCase
_start_with_underscore
numberAtTheEnd123
ALL_CAPS
```

## Semicolon
Semicolons aren't needed in WIP. But if you want, you can use them.

```wip
const system = import('system')

system.print('This is allowed')
system.print('This is allowed too');

```