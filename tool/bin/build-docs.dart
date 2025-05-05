import 'dart:io';

import 'package:glob/glob.dart';
import 'package:glob/list_local_fs.dart';
import 'package:markdown/markdown.dart';
import 'package:mime_type/mime_type.dart';
import 'package:path/path.dart' as p;
import 'package:shelf/shelf.dart' as shelf;
import 'package:shelf/shelf_io.dart' as io;
import 'package:tool/src/page.dart';
import 'package:tool/src/term.dart' as term;

const inputPath = 'docs';
const outputDir = 'build';

void main(List<String> arguments) async {
  _buildPages();

  if (arguments.contains("--serve")) {
    await _runServer();
  }
}

void _buildPages() {
  for (var entry in Glob(inputPath + '/**.md').listSync()) {
    if (entry is File) {
      _buildPage(new Page(entry as File));
      print("${term.green('Build ' + entry.path)}");
    }
  }
}

void _buildPage(Page page) {
  var mdContent = page.readMarkdown();
  var html = markdownToHtml(mdContent);
  var htmlFile = new File(page.htmlPath);
  htmlFile.createSync(recursive: true);
  htmlFile.writeAsString(html);
}

Future<void> _runServer() async {
  Future<shelf.Response> handleRequest(shelf.Request request) async {
    var filePath = p.normalize(p.fromUri(request.url));
    if (filePath == ".") filePath = "index.html";
    var extension = p.extension(filePath).replaceAll(".", "");

    // Refresh files that are being requested.
    if (extension == "html") {
      _buildPages();
    } else if (extension == "css") {
//      _buildSass(skipUpToDate: true);
    }

    try {
      var contents =
          await File(p.join("build", "docs", filePath)).readAsBytes();
      return shelf.Response.ok(contents, headers: {
        HttpHeaders.contentTypeHeader: mimeFromExtension(extension)!
      });
    } on FileSystemException {
      print(
          "${term.red(request.method)} Not found: ${request.url} ($filePath)");
      return shelf.Response.notFound("Could not find '$filePath'.");
    }
  }

  var handler = const shelf.Pipeline().addHandler(handleRequest);

  var server = await io.serve(handler, "localhost", 8000);
  print("Serving at http://${server.address.host}:${server.port}");
}
