import 'dart:io';

import 'package:glob/glob.dart';
import 'package:glob/list_local_fs.dart';
import 'package:tool/src/page.dart';

class Website {
  List<Page> pages = [];

  Stream<Page> getPages(List<String> globs) async* {
    for (var glob in globs) {
      for (var entry in Glob(glob).listSync()) {
        if (entry is File) {
          yield new Page();
        }
      }
    }
  }
}
