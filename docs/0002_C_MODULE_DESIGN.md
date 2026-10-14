# 0002 C Module Design

A C module design document must contain the following four sections.

## 1. Public Data Structures

- One entry per exported struct/enum/union: purpose and a field table (name, type, unit/range, description).
- For pointer fields: state who owns the memory and its lifetime.

## 2. Public API Overview

- One table listing every public function: signature, short description.

## 3. Public API Details

Per function, state:

- Parameter checks: preconditions and behavior on invalid input (return value / assert / ignore).
- Data lifetime: who allocates and frees, how long data stays valid, thread-safety constraints.
- Input/output behavior: observable result for each defined input class; this is the basis for test cases.

## 4. Typical Use Cases

- One minimal compilable example per main workflow.

## Document Skeleton

````markdown
# <module-name> Design

## 1. Public Data Structures

### `<type_name>`

Purpose: TODO

| Field | Type | Unit/Range | Description |
|-------|------|------------|-------------|
| TODO  | TODO | TODO       | TODO        |

## 2. Public API Overview

| Function | Signature | Description |
|----------|-----------|-------------|
| TODO     | TODO      | TODO        |

## 3. Public API Details

### `<function_name>`

- Parameter checks: TODO
- Data lifetime: TODO
- Input/output behavior: TODO

## 4. Typical Use Cases

```c
/* TODO: minimal working example */
```
````
