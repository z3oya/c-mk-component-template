# 0002 C Module Design

A C module design document must contain the following six sections.

## Writing Rules

- State each fact exactly once, in the section where it belongs.
- Usage constraints appear only in the interface details. Shared type mappings, parameter formats, and shared return codes appear only under Public Data Structures.
- Do not put information in parentheses: write it into the sentence, or move it to the section where it belongs.
- No meta-commentary such as evaluation order or design rationale.
- No emoji or composite Unicode characters.

## 1. Public Data Structures

- If the module exports no data structure, say so explicitly here.
- Define shared conventions here when several interfaces use them: type mappings, parameter formats, and the shared return-code table.
- Shared return codes: one table of code and meaning only. The interface entries state which codes each interface returns; the table does not.
- Show the full code definition of each exported struct/enum/union in a ```c fenced block.
- One entry per exported struct/enum/union: purpose and a field table with name, unit/range, and description.
- For pointer fields: state who owns the memory and its lifetime.

## 2. Public API Overview

- One table listing every public function: signature and a purpose phrase.
- The description states the purpose only. Behavior, return values, ownership, and constraints belong to section 3 and must not appear in this table.

## 3. Public API Details

- No summary subsections such as general conventions. Write every usage constraint into the entry of the interface it applies to.
- Per function, state:

  - Parameter checks: preconditions and behavior on invalid input, whether it returns a value, asserts, or ignores the input.
  - Data lifetime: who allocates and frees, how long data stays valid.
  - Input/output behavior: observable result for each defined input class; this is the basis for test cases.
  - Thread safety: locking scheme and caller obligations.

- A term used by several entries is defined at its first use.

## 4. Typical Use Cases

- One minimal compilable example per main workflow.
- The example centers on a main function; do not extract helper functions unless necessary.
- Do not chain several calls inside one condition.
- Comments serve only as placeholders for application logic; they must not restate document contracts.

## 5. Dependencies

- Library names only, no annotations.

## 6. Design Fence

- List capabilities intentionally excluded from this design.
- Purpose: keep the design minimal per YAGNI, make future extension points explicit, and give reviews a concrete target to challenge.
- Entries are design-level capability names only: no details, no annotations.
- Every entry opens with a negation: "No support for X", or a similar negated form such as "No X", "Not considering X", or "Not seeking X".
- A fence entry must not overlap with a capability the document body already includes.

## Document Skeleton

````markdown
# <module-name> Design

## 1. Public Data Structures

TODO: export statement; shared type mappings, parameter formats, and shared return codes

| Return value | Meaning |
|--------------|---------|
| TODO         | TODO    |

### `<type_name>`

TODO: purpose

```c
/* TODO: type definition */
```

| Field | Unit/Range | Description |
|-------|------------|-------------|
| TODO  | TODO       | TODO        |

## 2. Public API Overview

| Signature | Description |
|-----------|-------------|
| TODO      | TODO        |

## 3. Public API Details

### `<function_name>`

- Parameter checks: TODO
- Data lifetime: TODO
- Input/output behavior: TODO
- Thread safety: TODO

## 4. Typical Use Cases

```c
int main(void)
{
    /* TODO: minimal working example */
}
```

## 5. Dependencies

- TODO: library

## 6. Design Fence

| Excluded capability |
|---------------------|
| No support for TODO |
````
