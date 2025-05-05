import 'dart:io';

import 'package:glob/glob.dart';
import 'package:glob/list_local_fs.dart';
import 'package:markdown/markdown.dart';
import 'package:mime_type/mime_type.dart';
import 'package:mustache_template/mustache_template.dart';
import 'package:path/path.dart' as p;
import 'package:sass/sass.dart' as sass;
import 'package:shelf/shelf.dart' as shelf;
import 'package:shelf/shelf_io.dart' as io;
import 'package:tool/src/code_syntax.dart';
import 'package:tool/src/page.dart';
import 'package:tool/src/term.dart' as term;

const inputPath = 'docs';
const mustachePath = 'docs/assets/layouts';
const scssPath = 'docs/assets/scss';
const fontPath = 'docs/assets/fonts';
const imagePath = 'docs/assets/images';

const outputDir = 'build/docs';

void main(List<String> arguments) async {
  _clearBuild();

  _copy(fontPath);
  _copy(imagePath);
  _buildScss();
  _buildPages();

  if (arguments.contains("--serve")) {
    await _runServer();
  }
}

void _clearBuild() {
  if (Directory(outputDir).existsSync()) {
    Directory(outputDir).deleteSync(recursive: true);
  }
}

void _copy(String path) {
  for (var entry in Glob(p.join(path, '**')).listSync()) {
    var entryPath = p.normalize(entry.path);
    var fontPath = p.join(
        "./build", p.normalize(entry.parent.path), p.basename(entryPath));
    var destDir = p.join('build', p.normalize(entry.parent.path));

    Directory(destDir).createSync(recursive: true);
    File(entryPath).copy(fontPath);
  }
}

void _buildScss() {
  for (var entry in Glob(p.join(scssPath, '**.scss')).listSync()) {
    var scssPath = p.normalize(entry.path);
    var cssPath = p.join("./build", p.normalize(entry.parent.path),
        '${p.basenameWithoutExtension(scssPath)}.css');

    var result = sass.compileToResult(scssPath,
        color: true, style: sass.OutputStyle.expanded);
    var cssFile = new File(cssPath);
    cssFile.createSync(recursive: true);
    cssFile.writeAsStringSync(result.css);
    print("${term.green('-')} $cssPath");
  }
}

void _buildPages() {
  for (var entry in Glob(inputPath + '/**.md').listSync()) {
    if (entry is File) {
      _buildPage(new Page(entry as File));
      print("${term.green('Build ' + p.normalize(entry.path))}");
    }
  }
}

void _buildPage(Page page) {
  var mdContent = page.readMarkdown();
  var body = markdownToHtml(
    mdContent,
    extensionSet: ExtensionSet.gitHubFlavored,
    blockSyntaxes: [HighlightedCodeBlockSyntax()],
  );

  var data = <String, dynamic>{
    "title": page.title,
    "body": body,
  };
  var html = _loadMustache("default").renderString(data);

  var htmlFile = new File(page.htmlPath);
  htmlFile.createSync(recursive: true);
  htmlFile.writeAsStringSync(html);
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
      _buildScss();
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

Template _loadMustache(String name) {
  var path = p.join(mustachePath, "$name.html");
  return new Template(new File(path).readAsStringSync(),
      name: path, partialResolver: _loadMustache);
}
