# Move

## Implementation of analysis
>**NOTE:** This section is mostly compiler documentation and not that useful for language use. Better explanation will follow

Values _not_ implementing `core::copy::Copy` are dropped at the end of the declaring scope, unless they are moved.

There are rules dictating when a value is valid to move and when it is considered moved.
These rules primarily apply to variables since immediate values and function return values can only be accessed directly once.

1. By default a variable is free to be moved
2. A variable declared inside a block does not get affected by application of rules on outer blocks
3. After an if condition, the variable is considered moved if it is moved in at least one of the branches
4. A variable defined outside a loop may not be moved in the loop as it could be moved during the next iteration
5. A variable may be moved if the current block `break`s or `return`s unconditionally
    - a `return` inside a block that `break`s or `return`s still allows movement as it does not weaken exit beahvior
    - a `break` inside a block that `break`s still allows movement as it does not weaken exit behavior
    - a `break` inside a block `return`s prohibits movement as the return might be circumvented
    - a `continue` inside a block that `break`s or `return`s prohibits movement as the exit might be circumvented

Variables are stored in a stack and each know their position in the stack
###Stack flags and counters
- loop index/threshold
    - signals which variables were created inside the loop vs outside
- returning flag
    - signals that we are returning in this block or this block is returned
- breaking flag
    - signals that we are breaking in this block or this block is returned
- returing move
    - move done due to the returning flag, ideally a list of all but a single one is fine since we only report one error at a time anyways
- breaking move
    - move done due to the breaking flag, ideally a list of all but a single one is fine since we only report one error at a time anyways

### Loops
- store the current size of the stack as a threshold
- unset the unconditional break flag
- clear the old breaking move
- a variable with an index smaller than the threshold was created before the loop and may not be moved if
    - the unconditional return flag is not set
    - the unconditional break flag is not set
- after resolving the loop
    - restore the old threshold
    - restore the old break flag
    - restore the old breaking move 

### Conditionals
- create a copy of the stack for each branch
- after resolving each branch, join the stacks back together
    - a variable moved in at least one stack is also moved in the main stack
    - a variable nowhere moved remains valid and moveable

### Return
#### Returned blocks `return { ... };`
- the uncondititonal return flag is set
- the return move is reset
- the stack is copied
- we resolve the expression/block
- at the end
    - if the unconditional return flag is unset
        - we replace the stack variables with the copied stack variables
    - the unconditional return flag is restored to its previous state
    - the old return move is restored

#### Blocks containing unconditional (direct) return
- we need to look ahead to see whether we find a return or not
_the next steps are analogous to returned blocks_
- any variables after the return follow the same rules as if the block didn't contain return
- we continue evaluation as if we exited a returned block

### Break
_analogous to return_
- the returning invalid flag is set since a return might have been circumvented by this
- any previous move possible due to returning flag is invalid and creates an error

### Continue (blocks containing continue)
- the returning flag is unset since a return might have been circumvented by this
- the breaking flag is unset since a break might have been circumvented by this
- any previous move possible due to returning flag or breaking flag is invalid and creates an error