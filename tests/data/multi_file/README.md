# Test

Example bilog snippet — this file is Markdown, so preproc must ignore it:

```cpp
bilog::log({})
    .info("Should not appear in schema")
    .i("markdown_only_tag:", 42U)
    .cs("markdown_only_node:", "ghost")
    .write();
```
