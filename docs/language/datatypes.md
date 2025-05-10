# Datatypes

## Available Datatypes

| Type | Example |
| --- | --- |
| Null | null |
| Bool | true |
| Number | 42.5 |
| String | "Hello World" |
| Type | typeOf(null) |
| Class | class Greeter{} | 
| Object | Greeter.new() |
| Array | [] |
| Table | {} |

## Null
The type `Null` has only one single value: `null`.
It is usualy used to represent the absence of a Value.

```wip
var a = null
print(getType(a)) // prints: Null
```

## Bool
The type `Bool` has two values, `true` and `false`.
It is used to represent truth or falsehood.

```wip
var a = true
print(typeOf(a)) // prints: Bool
```

## Number
All numbers in WIP have the type `Number` and are double precision floating point numbers.

```wip
var a = 42
print(typeOf(a)) // prints: Number
```

## String
A string is a series of characters wrapped in singlequotes `'` or doublequtes `"` and have the type `String`. 
```wip
var a = 'Hello World'
print(typeOf(a)) // prints: String
```

If a string is wrapped in doublequotes `"`, it is a template string and values can be injected.
If the injected value is not a `String`, it will be casted to `String`:
```wip
var answer = 42
print("the answer to life the universe and everything is $answer") 
```

If you want to inject an more complex expression than a variable, you can wrap it in braces:
```wip
class User{
    public var name;

    construct new(name) {
        this.name = name;
    }
}

var user = User.new('Arno Nym')
print("Hello ${user.name}") // prints: 'Hello Arno Nym' 
```

## Class
Every class has the type `Class`. 

```wip

```