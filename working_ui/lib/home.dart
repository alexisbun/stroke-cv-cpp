import 'package:flutter/material.dart';
import 'package:stroke_action_awareness/menu.dart';

class Home extends StatefulWidget {
  const Home({super.key});

  @override
  State<Home> createState() => _HomeState();
}

class _HomeState extends State<Home> {
  // Main menu navigation.
  void goToPage() {
    Navigator.push(context, MaterialPageRoute(builder: (context) => Menu()));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("")),
      body: Center(child: Text("camera")),
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          FloatingActionButton(
            onPressed: () {},
            heroTag: "third",
            child: Icon(Icons.apps),
          ),
          FloatingActionButton(
            onPressed: goToPage,
            heroTag: "second",
            child: Icon(Icons.menu),
          ),
        ],
      ),
    );
  }
}
