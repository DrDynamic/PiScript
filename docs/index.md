# WIP is a class based language with some functional elements
WIP is similar to PHP with a sprinkle of JavaScript. 


This documentation ist a vision what WIP could be in the future. At the moment everything is subject to change. 


```wip
const sys = import('system');

class Greeter {
  private var greeting

  construct new(greeting) {
    this.greeting = greeting
  }

  fun greet(name) {
    sys.print("${this.greeting} ${name}")
  }
}

var greeter = Greeter.new("Hello")
greeter.greet("World")
```

- WIP aims to be fast
- WIP wants to be transpileable from PHP
- Maybe WIP will be transpoled to Javascript
- WIP already is class based
- Maybe WIP will be mit threaded 
