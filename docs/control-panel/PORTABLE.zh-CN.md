# 桌面控制配置的导出与导入

更新：2026-11-15。仅桌面端（Windows）功能，不修改跨端协议，Android 端无需改动。

## 入口

「桌面控制」页标题栏右侧新增「导出配置」「导入配置」两个按钮。

- **导出**：选择保存位置后写出一个自包含的 JSON 文档，默认扩展名 `.kdpanel.json`（若填写的路径没有 `.json` 后缀会自动补上）。结果为只读快照，不改动本机配置、不递增修订号。
- **导入**：选择文档后先弹出摘要（页面数、按钮数、图标数、每页明细、导出时间），确认「导入并替换」才真正写入。摘要阶段已完成完整校验，因此不会先破坏配置再报错。
- 导入前的原配置由 `PanelStore` 自动写入 `panel.json.bak`，导入后可点「撤回上次修改」恢复配置。

## 文档里有什么

文档是**完整搬迁**用的，包含 `panel.json` 的全部内容以及它引用的全部图标，因此换电脑、重装或备份恢复后不需要重新配一遍按钮。

| 段落 | 内容 |
| --- | --- |
| `format` / `formatVersion` | 固定为 `kdeconnect-controlpanel` / `1`，用于识别拒绝其他 JSON |
| `exportedAt` / `generator` | UTC 导出时间、生成端标识，仅供人阅读 |
| `summary` | 页面数、按钮数、图标数、导出时的修订号 |
| `config` | **完整的 `panel.json` 私有配置**：`schemaVersion`、`revision`、`mode`、`homePageId`、`activePageId`、`pages[]`、`appearance`、`idle` |
| `assets[]` | 所有被按钮引用的图标，每项含 `id`、`encoding`、`bytes`、`sha256`、`data` |

结构示意（占位符必须替换为真实值；实际文档的 `slots` 恰好 18 项，此处省略了中间的 `null`）：

```json
{
  "format": "kdeconnect-controlpanel",
  "formatVersion": 1,
  "exportedAt": "2026-11-15T12:46:18Z",
  "generator": "kdeconnect-controlpanel-desktop",
  "summary": { "pages": 2, "buttons": 4, "assets": 3, "revision": 17 },
  "config": {
    "schemaVersion": 1,
    "revision": 17,
    "mode": "pages",
    "homePageId": "home",
    "activePageId": "home",
    "pages": [
      {
        "id": "home",
        "name": "主页",
        "application": "",
        "slots": [
          {
            "id": "web",
            "name": "Bilibili",
            "kind": "website",
            "icon": "<该 PNG 内容的 SHA-256，64 位小写十六进制>",
            "target": { "url": "https://www.bilibili.com" }
          },
          { "id": "app", "name": "记事本", "kind": "program", "icon": "<另一张 PNG 的 SHA-256>", "target": { "path": "C:/Windows/notepad.exe" } }
        ]
      }
    ],
    "appearance": { "background": "#FFFFFF", "showLabels": true, "showVolume": true, "swipeDirection": "horizontal" },
    "idle": { "enabled": true, "timeoutMs": 5000, "dialId": "info", "city": "", "message": "" }
  },
  "assets": [
    {
      "id": "<与 data 解码内容 SHA-256 完全一致的 64 位小写十六进制>",
      "encoding": "base64",
      "bytes": 12345,
      "sha256": "<同 id>",
      "data": "<256×256 PNG 原始字节的严格 base64>"
    }
  ]
}
```

`config.pages[]` 保持原有顺序与全部字段：页面 `id` / `name` / `application`（前台应用绑定）、每页恰好 18 个 `slots[]`；每个按钮保留 `id` / `name` / `kind` / `icon` / `target`（程序路径、网址、快捷键、命令参数等桌面私有信息）。`idle` 里的表盘设置、天气城市、个性化文本、`info` 备注与 WorkBuddy 进度源配置也一并带走。首页与当前页 ID 原样保留，导入后定位到首页。

**不包含**：主题偏好（本机 QSettings）、天气快照、WorkBuddy 实时进度、配对关系、历史备份、`assets` 中已没有任何按钮引用的孤立图标。前两类是运行时状态或本机外观，不是配置；孤立图标不影响任何页面显示。因此导出体积通常远小于整个 assets 目录。

## 图标怎么带走

按钮的图标统一是内容寻址的 `assets/<sha256>.png`：256×256、透明补边、≤512 KiB，文件名就是文件内容 SHA-256。这一份资产同时覆盖：

- 添加网址时自动抓取的 favicon（`link rel=icon` / `/favicon.ico`）；
- 从 exe / lnk 提取的桌面应用图标；
- 手动选择的 JPG / PNG；
- 系统功能、文件、文件夹等由图标主题生成的默认图标。

导出时按「页面 → 槽位」顺序收集所有非空 `icon`，去重后把**原始 PNG 字节**做标准 base64 放进 `assets[].data`，不做二次压缩、不重新编码、不改文件名。因为 ID 就是字节的哈希，重新编码会让 ID 失效；导入时必须原样写回。

导入校验（`PanelPortable` + `PanelAssets::install`）：

1. 文档 ≤64 MiB、JSON 深度 ≤64、格式与版本正确；
2. `config` 用与 `panel.json` 完全相同的 `PanelDocument::validate` 校验（1–10 页、每页 18 槽、ID / 类型 / 颜色 / 表盘字段范围）；
3. 每个图标：`id` 为 64 位小写十六进制、`data` 为严格 base64（往返编码一致）、解码后 ≤512 KiB、PNG 签名 + 实际解码为 256×256、`SHA-256(内容) == id`，`sha256` / `bytes` 声明（若有）也必须吻合；
4. **交叉校验**：每个页面每个槽位引用的图标，必须能在文档内找到；若文档里没有但本机已有同 ID 资产则复用，两者都没有就拒绝导入，避免恢复出一堆空白图标；
5. 通过后才逐个安装图标（已存在的按内容去重跳过），最后原子写入 `panel.json`。

图标是「只增不删」：导入不会删除本机原有图标，也不会清空 assets。若配置写入在图标安装之后失败，只会留下若干无人引用的 PNG（内容寻址、无副作用），配置保持原样。

## 导入后的刷新机制

1. `PanelStore` 先写 `revision` 计数文件，再用 `QSaveFile` 原子替换 `panel.json`，同时保留 `.bak`。**文档里的 `revision` 不会被沿用**：导入时被替换为本机当前修订号，随后由 store 统一分配 `max(计数器, 内存, 磁盘) + 1`，保证单调、不与另一窗口冲突，也不会触发 `revision_rollback` / `revision_collision`。
2. `PanelStore::changed` 会让 `PanelEditor` 重新发出 `configChanged` / `changed`，QML 的页面列表、槽位网格、表盘设置、外观设置全部重新绑定；当前选中页切到导入配置的首页。天气模块与 WorkBuddy 进度源按新的 `idle` 配置重新 `configure()`。
3. 已配对的手机不会收到导入文档本身：桌面插件在下一次配置广播 / 重新同步时按新修订号下发 `publicConfig()`，手机再按需要请求缺失图标资源。
4. 导出是快照，不含任何自动同步；再次导出即可得到最新状态。

## 手工编辑文档

文档是缩进格式的普通 JSON，可以直接用文本编辑器修改 `config` 段（例如批量改网址、调整页面名称、修正跨电脑后的程序路径）：

- `format`、`formatVersion`、`config`、`assets` 四个键必须保留；`summary` / `exportedAt` / `generator` 是注释性质的，可删。
- `config.revision` 可以随意填写或删除，导入时一律忽略。
- 新增按钮的 `icon` 必须对应 `assets` 里的一项；想去掉图标就把 `icon` 设为空字符串。
- 想替换图标，需要自行生成 256×256 PNG、计算其 SHA-256 作为 `id` 与 `sha256`、把字节 base64 填进 `data`、`bytes` 填解码后长度。任一处不吻合都会被拒绝，不会静默使用错误图片。
- 程序 / 文件路径、快捷方式目标**不会校验是否存在**；跨电脑导入后请在页面里重新指定，网址与快捷键可直接沿用。

## 限制与安全

- 文档含程序绝对路径、网址和备注，可能携带个人信息，请按普通私人文件保管；导入前会显示摘要，但不会逐个列出路径。
- 导出时若本机有按钮引用的图标文件损坏或丢失，会明确失败并提示先「补齐默认图标」，不会导出缺图配置。
- 上限：文档 64 MiB、最多 180 个图标（10 页 × 18 槽）、单个图标 512 KiB、配置段 256 KiB。assets 仓库仍受 128 MiB 总量限制。
- 读取文档只接受本地普通文件（拒绝符号链接），不会执行脚本、不发起网络请求、不跟随快捷方式。
- 主题偏好、天气与工作进度等运行时状态不随文档迁移。

相关：图标规范见 [控制面板图标](ASSETS.zh-CN.md)，配置字段见 [协议](PROTOCOL.zh-CN.md)，信息表盘右侧配置见 [INFO](INFO.zh-CN.md)。
