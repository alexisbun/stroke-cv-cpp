import 'package:flutter/material.dart';

class Menu extends StatefulWidget {
  const Menu({super.key});

  @override
  State<Menu> createState() => _MenuState();
}

class _MenuState extends State<Menu> {
  void goBack() {
    Navigator.pop(context);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text("Main Menu"),
        automaticallyImplyLeading: false,
      ),
      body: HomeContent(),
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          OutlinedButton(
            onPressed: () {},
            // Create an outlined button default style method for those two.
            style: OutlinedButton.styleFrom(
              side: const BorderSide(
                width: 1.0,
                color: Color.fromARGB(255, 192, 192, 192),
              ),
            ),
            child: Icon(Icons.settings),
          ),
          OutlinedButton(
            onPressed: goBack,
            style: OutlinedButton.styleFrom(
              side: const BorderSide(
                width: 1.0,
                color: Color.fromARGB(255, 192, 192, 192),
              ),
            ),
            child: Text('Back'),
          ),
        ],
      ),
    );
  }
}

class HomeContent extends StatefulWidget {
  const HomeContent({super.key});

  @override
  State<HomeContent> createState() => _HomeContentState();
}

class _HomeContentState extends State<HomeContent> {
  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      spacing: 3,

      // REWRITE: Create a button style default 'widget' / method and pass in the text as arguments.
      children: <Widget>[
        FilledButton.tonal(
          onPressed: () {},
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(vertical: 25),
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(12)),
            ),
          ),
          child: const Text('Learn the Basics of Stroke Action Awareness'),
        ),
        FilledButton.tonal(
          onPressed: () {},
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(vertical: 25),
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(12)),
            ),
          ),
          child: const Text(
            'How to Recognize Stroke: Guided Tutorial Using AR',
          ),
        ),
        FilledButton.tonal(
          onPressed: () {},
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(vertical: 25),
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(12)),
            ),
          ),
          child: const Text(
            'AR-Based Evaluation: Act FAST When You See Stroke',
          ),
        ),
        FilledButton.tonal(
          onPressed: () {},
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(vertical: 25),
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(12)),
            ),
          ),
          child: const Text('Stroke Treatment and Importance of Acting FAST'),
        ),
        FilledButton.tonal(
          onPressed: () {},
          style: FilledButton.styleFrom(
            padding: const EdgeInsets.symmetric(vertical: 25),
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(12)),
            ),
          ),
          child: const Text('How to Learn More?'),
        ),
      ],
    );
  }
}

// class DefaultButtonStyling extends StatelessWidget {
//   const DefaultButtonStyling({super.key});

//   @override
//   Widget build(BuildContext context) {
//     return const ;
//   }
// }

// style: ButtonStyle(
//             backgroundColor: WidgetStateProperty.resolveWith<Color?>((
//               Set<WidgetState> states,
//             ) {
//               if (states.contains(WidgetState.pressed)) {
//                 return Theme.of(
//                   context,
//                 ).colorScheme.primary.withValues(alpha: 0.5);
//               }
//               return null;
//             }),
//           ),
