using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CoroutineResult<T>
{
    public T Result { get; private set; }
    public Coroutine Coroutine { get; private set; }

    public CoroutineResult(MonoBehaviour owner, IEnumerator routine)
    {
        Coroutine = owner.StartCoroutine(Run(routine));
    }

    private IEnumerator Run(IEnumerator routine)
    {
        while (routine.MoveNext())
        {
            Result = (T)routine.Current;
            yield return Result;
        }
    }
}