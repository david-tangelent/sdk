// Copyright (c) 2020, the Dart project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'package:analysis_server/protocol/protocol.dart';
import 'package:analysis_server/protocol/protocol_generated.dart'
    hide AnalysisOptions;
import 'package:analysis_server/src/analysis_server.dart';
import 'package:analysis_server/src/domain_analysis.dart';
import 'package:analysis_server/src/domain_completion.dart';
import 'package:analysis_server/src/server/crash_reporting_attachments.dart';
import 'package:analysis_server/src/utilities/mocks.dart';
import 'package:analysis_server/src/utilities/progress.dart';
import 'package:analyzer/file_system/file_system.dart';
import 'package:analyzer/file_system/memory_file_system.dart';
import 'package:analyzer/instrumentation/instrumentation.dart';
import 'package:analyzer/src/dart/analysis/driver.dart';
import 'package:analyzer/src/generated/engine.dart';
import 'package:analyzer/src/generated/sdk.dart';
import 'package:analyzer/src/test_utilities/mock_sdk.dart';
import 'package:matcher/matcher.dart';
import 'package:meta/meta.dart';

import '../../mocks.dart';

abstract class AbstractClient {
  final MockServerChannel serverChannel;
  final TestPluginManager pluginManager;
  late AnalysisServer server;

  final List<GeneralAnalysisService> generalServices =
      <GeneralAnalysisService>[];
  final Map<AnalysisService, List<String>> analysisSubscriptions = {};

  final String projectPath;
  final String testFilePath;
  late String testCode;

  final AnalysisServerOptions serverOptions;

  AbstractClient({
    required this.projectPath,
    required this.testFilePath,
    required String sdkPath,
    required this.serverOptions,
  })  : serverChannel = MockServerChannel(),
        pluginManager = TestPluginManager() {
    server = createAnalysisServer(sdkPath);
    var notificationStream = serverChannel.notificationController.stream;
    notificationStream.listen((Notification notification) {
      processNotification(notification);
    });
  }

  AnalysisDomainHandler get analysisHandler =>
      server.handlers.singleWhere((handler) => handler is AnalysisDomainHandler)
          as AnalysisDomainHandler;

  AnalysisOptions get analysisOptions => testDriver.analysisOptions;

  CompletionDomainHandler get completionHandler =>
      server.handlers.whereType<CompletionDomainHandler>().single;

  MemoryResourceProvider get resourceProvider;

  AnalysisDriver get testDriver => server.getAnalysisDriver(testFilePath)!;

  void addAnalysisOptionsFile(String content) {
    newFile(
        resourceProvider.pathContext.join(projectPath, 'analysis_options.yaml'),
        content);
  }

  void addAnalysisSubscription(AnalysisService service, String file) {
    // add file to subscription
    var files = analysisSubscriptions[service];
    if (files == null) {
      files = <String>[];
      analysisSubscriptions[service] = files;
    }
    files.add(file);
    // set subscriptions
    var request =
        AnalysisSetSubscriptionsParams(analysisSubscriptions).toRequest('0');
    handleSuccessfulRequest(request);
  }

  void addGeneralAnalysisSubscription(GeneralAnalysisService service) {
    generalServices.add(service);
    var request =
        AnalysisSetGeneralSubscriptionsParams(generalServices).toRequest('0');
    handleSuccessfulRequest(request);
  }

  String addTestFile(String content) {
    newFile(testFilePath, content);
    testCode = content;
    return testFilePath;
  }

  void assertValidId(String id) {
    expect(id, isNotNull);
    expect(id.isNotEmpty, isTrue);
  }

  /// Create an analysis server with the given [sdkPath].
  AnalysisServer createAnalysisServer(String sdkPath) {
    createMockSdk(
      resourceProvider: resourceProvider,
      root: newFolder(sdkPath),
    );
    return AnalysisServer(
        serverChannel,
        resourceProvider,
        serverOptions,
        DartSdkManager(sdkPath),
        CrashReportingAttachmentsBuilder.empty,
        InstrumentationService.NULL_SERVICE);
  }

  /// Create a project at [projectPath].
  @mustCallSuper
  Future<void> createProject({Map<String, String>? packageRoots}) async {
    newFolder(projectPath);

    await setRoots(
        included: [projectPath], excluded: [], packageRoots: packageRoots);
  }

  void expect(actual, matcher, {String reason});

  /// Validate that the given [request] is handled successfully.
  Response handleSuccessfulRequest(Request request, {RequestHandler? handler}) {
    handler ??= analysisHandler;
    var response = handler.handleRequest(request, NotCancelableToken())!;
    expect(response, isResponseSuccess(request.id));
    return response;
  }

  File newFile(String path, String content);

  Folder newFolder(String path);

  void processNotification(Notification notification);

  Future<Response> setRoots({
    required List<String> included,
    required List<String> excluded,
    Map<String, String>? packageRoots,
  }) async {
    var request = AnalysisSetAnalysisRootsParams(included, excluded,
            packageRoots: packageRoots)
        .toRequest('0');
    var response = await waitResponse(request);
    expect(response, isResponseSuccess(request.id));
    return response;
  }

  /// Returns a [Future] that completes when the server's analysis is complete.
  Future waitForTasksFinished() => server.onAnalysisComplete;

  /// Completes with a successful [Response] for the given [request].
  /// Otherwise fails.
  Future<Response> waitResponse(Request request,
      {bool throwOnError = true}) async {
    return serverChannel.sendRequest(request, throwOnError: throwOnError);
  }
}
