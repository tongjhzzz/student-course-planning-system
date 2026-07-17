# 智能课程规划系统数据集

这份数据集用于课程规划、选课推荐、时间冲突检测、先修关系检查和筛选功能测试。数据包含课程、教学班、开课时间、教师、教室、容量、先修关系和样例专业约束。

## 文件

- `course_info.csv`：课程信息表，3000 条课程班级记录，覆盖 1000 种基础课程。
- `course_time.csv`：开课时间表，每条课程班级至少 1 条上课时间记录。
- `sample_constraints.json`：计算机科学与技术专业样例规划约束，包含显式目标课程集合、每学期学分上限、避开时间段、偏好时间段等。
- `dataset_stats.json`：生成后的统计信息。
- `major_profiles.json`：多专业培养要求样例索引。
- `major_profiles/`：每个专业单独的规划约束 JSON。
- `course_visualization.html`：本地可打开的课程数据可视化页面。
- `major_profiles.html`：多专业培养要求样例可视化汇总。

## 数据来源说明

数据中的部分计算机专业核心课程名称、课程代码、学分参考公开培养方案，例如 `COMS0031121009 数据结构`。课程池额外覆盖教育学、中文、历史、哲学、法学、社会学、心理学、学前教育、特殊教育、小学教育、音乐、美术、设计、体育、地理、生命科学等方向。默认约束文件通过 `target_course_scope.required_course_basic_IDs` 指定目标专业课程集合，`required_categories_for_display` 用于展示课程类别。`recommended_term` 表示建议/最早修读学期，不是固定学期；课程可后移到同季节的更晚学期。

参考来源：

- 华东师范大学计算机科学与技术专业培养方案：`https://www.cs.ecnu.edu.cn/_upload/article/files/39/fd/336e581e45d1b9e7291670999437/5f9f76af-2606-40b8-a7bb-0ae38fe92f52.pdf`
- 华东师范大学本科生院公开课程/选课通知页面：`https://bksy.ecnu.edu.cn/`

## `course_info.csv` 字段

| 字段 | 说明 |
| --- | --- |
| `course_basic_ID` | 课程基础序号，同一门课不同教学班共享该字段 |
| `course_sp_ID` | 课程班级序号，和 `course_basic_ID` 组成教学班唯一键 |
| `course_name` | 课程名称 |
| `department` | 开课院系 |
| `semester` | `秋季学期` 或 `春季学期` |
| `recommended_term` | 建议/最早修读学期，取值 1 到 8，可后移到同季节后续学期 |
| `category` | `专业必修课`、`学科基础课`、`专业选修课` |
| `credit` | 学分 |
| `beg_week` | 开始周数 |
| `last_week` | 持续周数 |
| `teacher` | 授课教师，合成字段 |
| `classroom` | 上课教室，合成字段 |
| `limits` | 选课人数上限 |
| `prereq_ID` | 前序课程的 `course_basic_ID`，多门先修课用英文分号 `;` 分隔，空值表示无先修课 |

## `course_time.csv` 字段

| 字段 | 说明 |
| --- | --- |
| `course_basic_ID` | 课程基础序号 |
| `course_sp_ID` | 课程班级序号 |
| `day` | 上课日期缩写：`Mon`、`Tue`、`Wed`、`Thu`、`Fri`、`Sat`、`Sun` |
| `beg` | 开始节数 |
| `last` | 持续节数 |
