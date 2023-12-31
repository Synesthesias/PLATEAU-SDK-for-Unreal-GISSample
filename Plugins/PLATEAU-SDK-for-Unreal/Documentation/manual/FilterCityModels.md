# 3D都市モデルのフィルタリング
このページでは3D都市モデルに含まれる各地物のフィルタリング方法を記載します。

## モデル調整タブ
- Unrealのメニューバーから `PLATEAU` → `PLATEAU SDK` を選択します。
- ウィンドウ上部のタブのうち `モデル調整` を選択します。
![](../resources/manual/filterCityModels/filterTab.png)
- この画面で条件指定をしてフィルタリングをおこなえます。
- ここでいうフィルタリングとは、条件に合致するゲームオブジェクトをアクティブにし、そうでないものを非アクティブにすることを指します。
- 「重複する地物を表示」にチェックを入れた場合、フィルタリング後に重複している地物について、  
  もっともLODが高いもののみを残してそれ以外を非表示にします。
- 表示したい地物の種類をチェックボックスで指定します。
- チェックボックスは入れ子構造になっています。
  - 第1階層のチェックボックスは、「建築物」「道路」などのパッケージ種別を指定します。
  - 複数のLODがシーン中に存在する場合、パッケージ種別ごとにLOD範囲選択のスライダーを使ってLODを指定できます。
  - 第2階層のチェックボックスは、「ドア」「屋根」など細かい都市オブジェクト分類での種別を指定します。

> [!NOTE]  
> 都市インポート時のメッシュ結合単位の設定によっては、第2階層の分類チェックマークが動作しない場合があります。  
> 例えば、インポート時に建物を「最小地物単位」に指定した場合、「窓」「屋根面」などでゲームオブジェクトが分かれているので、細かい分類のチェックマークが動作します。  
> しかし、建物を「主要地物単位」にした場合、建物ごとにゲームオブジェクトが結合されているので、細かい「窓」「屋根面」といった分類は動作しません。  
> 分類指定のチェックマークは入れ子構造になっていますが、第1階層の「建築物」「道路」といった分類は結合単位によらず必ず動作し、  
> 第2階層の「窓」「屋根面」といった細かい分類はインポート時に「最小値物単位」にした場合のみ動作します。