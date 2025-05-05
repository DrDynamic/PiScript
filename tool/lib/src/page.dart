import 'dart:io';
import 'package:path/path.dart' as p;

class Page {
  File mdFile;

  Page(this.mdFile);

  String get title => this.name;

  String get name => p.basenameWithoutExtension(this.mdFile.path);
  String get htmlPath => p.join(
      "./build", p.normalize(this.mdFile.parent.path), '${this.name}.html');

  String readMarkdown() {
    return this.mdFile.readAsStringSync();
  }
}
