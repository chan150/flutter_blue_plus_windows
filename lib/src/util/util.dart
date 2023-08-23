import 'dart:async';

// This is a reimplementation of BehaviorSubject from RxDart library.
// It is essentially a stream but:
//  1. we cache the latestValue of the stream
//  2. the "latestValue" is re-emitted whenever the stream is listened to
class CachedStreamController<T> {
  T latestValue;

  final StreamController<T> _controller = StreamController<T>.broadcast();

  CachedStreamController({required T initialValue})
      : this.latestValue = initialValue;

  Stream<T> get stream => _controller.stream;

  T get value => latestValue;

  void add(T newValue) {
    latestValue = newValue;
    _controller.add(newValue);
  }

  void listen(
    Function(T) onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    onData(latestValue);
    _controller.stream.listen(
      onData,
      onError: onError,
      onDone: onDone,
      cancelOnError: cancelOnError,
    );
  }

  Future<void> close() {
    return _controller.close();
  }
}

// add to list if item is new,
// or update existing item
List<T> addOrUpdate<T>(List<T> results, T item) {
  var list = List<T>.from(results);
  if (list.contains(item)) {
    int index = list.indexOf(item);
    list[index] = item;
  } else {
    list.add(item);
  }
  return list;
}
