eos3d.io 和 eosday.io 智能合约源代码
----

为了让玩家放心，我们完全开放了源代码，并且主动放弃了对于智能合约的任何控制权限。

## 短版(6小时 + 1分钟版):

* 网站地址: [https://eos3d.io](https://eos3d.io/)
* EOS 智能合约账户: **eos3dio12345**
* 源代码: [eos3d12345](https://github.com/yanxi-me/eos3d-contract/tree/master/eos3dio12345)

## 长版(24小时 + 1小时版):

* 网站地址: [https://eosday.io](https://eosday.io/)
* EOS 智能合约账户: **eosdayeosday**
* 源代码: [eosdayeosday](https://github.com/yanxi-me/eos3d-contract/tree/master/eosdayeosday)


## 如何确认合约权限已经移交

```
$ cleosm get account eos3dio12345
permissions:
     owner     1:    1 eosio.prods@active,
        active     1:    1 eos3dio12345@eosio.code,
memory:
...
```

```
$ cleosm get account eosdayeosday
permissions:
     owner     1:    1 eosio.prods@active,
        active     1:    1 eosdayeosday@eosio.code,
memory:
...
```

通过 `get account` 命令可以查看合约账户权限，owner 权限已经移交给了 eosio.prods，即所有的出块节点（BP)，active 权限为合约代码本身，这是为了可以利用合约实现自动提币。开发团队已经对合约没有任何控制权限。
