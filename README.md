# Python C Extension Example - calcimp

このプロジェクトは、PythonのC拡張モジュールの例を示しています。

## ビルド方法

```bash
python setup.py build
```

## インストール方法

```bash
python setup.py install
```

## 使用例

```python
import calcimp
result = calcimp.add(5, 3)  # returns 8
```

## 要件
- Python 3.6以上
- C コンパイラ（gcc推奨）
