```ini
python-path=.venv/
models-path=resources/models/path
plan-path=docs/angeloid-alpha-plan.md
```
不要忽略.gitignore中过滤的文件。

解决问题时不要规避，要正面解决问题。

## Git提交规则

提交代码时，使用 Co-authored-by 方式标识 AI 助手贡献：

```
Co-authored-by: GLM-5 <noreply@zhipu.ai>
```

示例提交信息格式：
```
feat: 简短描述

- 详细说明1
- 详细说明2

Co-authored-by: GLM-5 <noreply@zhipu.ai>
```

## 提交之前待办
* 更新 plan.md，确保与代码同步更新，同时更新README.md
* 检查本rule文件是否也committed，如果没有需要提交
* 如果更新了README.md，请同步检查README_EN.md