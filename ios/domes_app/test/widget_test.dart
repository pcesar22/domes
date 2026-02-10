import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:domes_app/main.dart';

void main() {
  testWidgets('App launches with DOMES title', (WidgetTester tester) async {
    await tester.pumpWidget(const ProviderScope(child: DomesApp()));
    expect(find.text('DOMES'), findsOneWidget);
  });
}
