# ecc

https://www.sigbus.info/compilerbook を参考にCコンパイラ作成しています。

## セルフホスト

セルフコンパイルしてコンパイラを作成
```
make stage2/ecc
```

セルフコンパイルでできたコンパイラでコンパイラ作成
```
make stage3/ecc
```

### テスト

第一世代
```
make test
```

第二世代
```
make test-stage2
```

第三世代
```
make test-stage3
```
