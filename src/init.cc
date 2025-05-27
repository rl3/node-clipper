#define DEBUG

#include <iostream>
#include <node.h>
#include <v8.h>
#include "clipper.hpp"
#include "clipper.cpp"

namespace demo
{

    using v8::Array;
    using v8::Boolean;
    using v8::Exception;
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::String;
    using v8::Value;

    using namespace ClipperLib;

    const unsigned long doubleFactor = 0x4000000000000;
    int debug = 0;

    void setDebug(const FunctionCallbackInfo<Value> &args)
    {
        if (args.Length() > 0 && args[0]->IsNumber())
        {
            debug = args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
        }
        args.GetReturnValue().Set(debug);
    }

    Paths v8ArrayToPolygons(Local<Array> inOutPolygons, bool doubleType)
    {
        Isolate *isolate = Isolate::GetCurrent();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        int len = inOutPolygons->Length();
        Paths polyshape(len);
#ifdef DEBUG
        if (debug > 1)
            std::cout << "polygonArray: length: " << len << std::endl;
#endif
        if (len < 1)
            return polyshape;

        for (int i = 0; i < len; i++)
        {
            Local<Array> polyLine = Local<Array>::Cast(inOutPolygons->Get(context, i).ToLocalChecked());
#ifdef DEBUG
            if (debug > 2)
                std::cout << "polyLine: length: " << polyLine->Length() << std::endl;
#endif
            if (polyLine->Length() < 3)
                continue;
            for (unsigned int j = 0; j < polyLine->Length(); j++)
            {
                Local<Array> point = Local<Array>::Cast(polyLine->Get(context, j).ToLocalChecked());
                if (point->Length() < 2)
                    continue;
                Local<Value> x = point->Get(context, 0).ToLocalChecked();
                Local<Value> y = point->Get(context, 1).ToLocalChecked();
                IntPoint p;
                if (doubleType)
                {
                    p = IntPoint(
                        x->NumberValue(context).FromMaybe(0) * doubleFactor,
                        y->NumberValue(context).FromMaybe(0) * doubleFactor);
                }
                else
                {
                    p = IntPoint(
                        x->NumberValue(context).FromMaybe(0),
                        y->NumberValue(context).FromMaybe(0));
                }
                polyshape[i].push_back(p);
#ifdef DEBUG
                if (debug > 3)
                    std::cout << "polyLine: point: " << p.X << " " << p.Y << std::endl;
#endif
            }
        }
        return polyshape;
    }

    long signedSum(long a, long b)
    {
        if ((a < 0 && b < 0) || (a > 0 && b > 0) || a == 0 || b == 0)
        {
            return a + b;
        }
        return a - b;
    }

    Local<Array> polygonsToV8Array(Isolate *isolate, Paths polygons, bool doubleType)
    {
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        Local<Array> result = Array::New(isolate);
        for (unsigned int i = 0; i < polygons.size(); i++)
        {
            Local<Array> points = Array::New(isolate);
            for (unsigned int k = 0; k < polygons[i].size(); k++)
            {
                IntPoint ip = polygons[i][k];
                Local<Number> x;
                Local<Number> y;
                if (doubleType)
                {
                    x = Number::New(isolate, (double)ip.X / (double)doubleFactor);
                    y = Number::New(isolate, (double)ip.Y / (double)doubleFactor);
                }
                else
                {
                    x = Number::New(isolate, ip.X);
                    y = Number::New(isolate, ip.Y);
                }
                Local<Array> point = Array::New(isolate);
                point->Set(context, point->Length(), x).FromJust();
                point->Set(context, point->Length(), y).FromJust();
                points->Set(context, points->Length(), point).FromJust();
            }
            result->Set(context, result->Length(), points).FromJust();
        }
        return result;
    }

    void doFixOrientation(Paths &polyshape)
    {
        if (!Orientation(polyshape[0]))
        {
            ReversePath(polyshape[0]);
#ifdef DEBUG
            if (debug > 0)
                std::cout << "doFixOrientation: outerPoints reversed" << std::endl;
#endif
        }
        for (unsigned int i = 1; i < polyshape.size(); i++)
        {
            if (Orientation(polyshape[i]))
            {
                ReversePath(polyshape[i]);
#ifdef DEBUG
                if (debug > 0)
                    std::cout << "doFixOrientation: innerPoints reversed: " << i - 1 << std::endl;
#endif
            }
        }
    }

    Local<String> checkArguments(const FunctionCallbackInfo<Value> &args, int checkLength)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        Local<String> result = String::Empty(isolate);
        if (args.Length() < 2)
        {
            result = String::NewFromUtf8(isolate, "Too few arguments! At least 'polyshape[][][]' and 'pointType' are required!", v8::NewStringType::kNormal).ToLocalChecked();
            return result;
        }
        if (args.Length() < checkLength)
        {
            result = String::NewFromUtf8(isolate, "Too few arguments!", v8::NewStringType::kNormal).ToLocalChecked();
            return result;
        }
        if (!args[0]->IsArray())
        {
            result = String::Concat(
                String::NewFromUtf8(isolate, "Wrong argument 'polyshape': array[shapes][points][point] required: ", v8::NewStringType::kNormal).ToLocalChecked(),
                args[0]->ToString(context).ToLocalChecked());
            return result;
        }
        if (checkLength < 2)
        {
            return result;
        }
        if (!args[1]->IsString() ||
            !(args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()) ||
              args[1]->StrictEquals(String::NewFromUtf8(isolate, "integer", v8::NewStringType::kNormal).ToLocalChecked())))
        {
            result = String::Concat(
                String::NewFromUtf8(isolate, "Wrong argument 'pointType': 'double' || 'integer' required: ", v8::NewStringType::kNormal).ToLocalChecked(),
                args[1]->ToString(context).ToLocalChecked());
            return result;
        }
        if ((args.Length() > 2) && (checkLength > 2))
        {
            if (!args[2]->IsNumber())
            {
                result = String::Concat(
                    String::NewFromUtf8(isolate, "Wrong argument 'delta' || 'distance': number required: ", v8::NewStringType::kNormal).ToLocalChecked(),
                    args[2]->ToString(context).ToLocalChecked());
                return result;
            }
        }
        if ((args.Length() > 3) && (checkLength > 3))
        {
            if (!args[3]->IsString() ||
                !(args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtMiter", v8::NewStringType::kNormal).ToLocalChecked()) ||
                  args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtSquare", v8::NewStringType::kNormal).ToLocalChecked()) ||
                  args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtRound", v8::NewStringType::kNormal).ToLocalChecked())))
            {
                result = String::Concat(
                    String::NewFromUtf8(isolate, "Wrong argument 'joinType': 'jtMiter' || 'jtSquare' || 'jtRound' required: ", v8::NewStringType::kNormal).ToLocalChecked(),
                    args[3]->ToString(context).ToLocalChecked());
                return result;
            }
        }
        if ((args.Length() > 4) && (checkLength > 4))
        {
            if (!args[4]->IsNumber())
            {
                result = String::Concat(
                    String::NewFromUtf8(isolate, "Wrong argument 'miterLimit': number required: ", v8::NewStringType::kNormal).ToLocalChecked(),
                    args[4]->ToString(context).ToLocalChecked());
                return result;
            }
        }
        return result;
    }

    void orientation(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        if (polyshape.size() <= 0)
        {
            return;
        }
        Local<Array> orientations = Array::New(isolate);
        for (unsigned int i = 0; i < polyshape.size(); i++)
        {
            bool polyOrientation = Orientation(polyshape[i]);
            orientations->Set(context, orientations->Length(), Boolean::New(isolate, polyOrientation)).FromJust();
        }
        args.GetReturnValue().Set(orientations);
    }

    void offset(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        JoinType joinType = jtMiter;
        EndType endType = etClosed;
        double miterLimit = 30.0;
        long delta;
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 3);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        if (doubleType)
        {
            delta = args[2]->NumberValue(context).FromMaybe(0) * doubleFactor;
        }
        else
        {
            delta = args[2]->NumberValue(context).FromMaybe(0);
        }
#ifdef DEBUG
        if (debug > 0)
            std::cout << "args[2]: delta: " << delta << std::endl;
#endif
        if (args.Length() > 3)
        {
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtMiter", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtMiter;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtSquare", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtSquare;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtRound", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtRound;
            }
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[3]: joinType: " << *v8::String::Utf8Value(isolate, args[3]) << std::endl;
#endif
        }
        if (args.Length() > 4)
        {
            miterLimit = args[4]->NumberValue(context).FromMaybe(30.0);
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[4]: miterLimit: " << miterLimit << std::endl;
#endif
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        doFixOrientation(polyshape);
        Paths polyshapeOut;
#ifdef DEBUG
        if (debug > 1)
            std::cout << "before Offset: polyshape.size(): " << polyshape.size() << std::endl;
#endif
        OffsetPaths(polyshape, polyshapeOut, delta, joinType, endType, miterLimit);
#ifdef DEBUG
        if (debug > 1)
            std::cout << "after  Offset: polyshapeOut.size(): " << polyshapeOut.size() << std::endl;
#endif
        if (polyshapeOut.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshapeOut, doubleType));
        }
    }

    void minimum(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        const unsigned long ScaleMax = doubleFactor - 1;
        JoinType joinType = jtMiter;
        EndType endType = etClosed;
        double miterLimit = 30.0;
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        if (args.Length() > 3)
        {
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtMiter", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtMiter;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtSquare", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtSquare;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "jtRound", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                joinType = jtRound;
            }
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[3]: joinType: " << *v8::String::Utf8Value(isolate, args[3]) << std::endl;
#endif
        }
        if (args.Length() > 4)
        {
            miterLimit = args[4]->NumberValue(context).FromMaybe(30.0);
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[4]: miterLimit: " << miterLimit << std::endl;
#endif
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        if (polyshape.size() <= 0)
        {
            return;
        }
        doFixOrientation(polyshape);
        Paths polyshapeOut(polyshape.size());
        long xMin = 0, xMax = 0, yMin = 0, yMax = 0, xScale = 0, yScale = 0, xyScale = 0;
        for (unsigned int i = 0; i < polyshape[0].size(); i++)
        {
            IntPoint ip = polyshape[0][i];
            if (i == 0)
            {
                xMin = ip.X;
                xMax = ip.X;
                yMin = ip.Y;
                yMax = ip.Y;
                continue;
            }
            if (ip.X < xMin)
                xMin = ip.X;
            if (ip.X > xMax)
                xMax = ip.X;
            if (ip.Y < yMin)
                yMin = ip.Y;
            if (ip.Y > yMax)
                yMax = ip.Y;
        }
#ifdef DEBUG
        if (debug > 1)
            std::cout << "polyshape[0]: boundingBox: " << xMin << " " << yMin << " " << xMax << " " << yMax << std::endl;
#endif
        xScale = signedSum(xMin, xMax) / 2;
        yScale = signedSum(yMin, yMax) / 2;
        xyScale = std::min(std::abs(xScale), std::abs(yScale));
        int s = -1;
        int loops = 0;
        long scale = xyScale;
#ifdef DEBUG
        if (debug > 1)
            std::cout << "OffsetPaths: xyScale: " << xyScale << std::endl;
#endif
        long lastScale = 2;
        do
        {
            loops++;
            try
            {
                OffsetPaths(polyshape, polyshapeOut, s * scale, joinType, endType, miterLimit);
            }
            catch (...)
            {
                miterLimit /= 2;
                if (miterLimit < 5)
                {
                    miterLimit = 5;
                }
                std::cout << "node-clipper: exception from ClipperLib catched! miterLimit reduced: " << miterLimit << std::endl;
                continue;
            }
#ifdef DEBUG
            if (debug > 1)
                std::cout << "polyshapeOut.size():    " << polyshapeOut.size() << std::endl;
            if (debug > 1)
                std::cout << "polyshapeOut[0].size(): " << polyshapeOut[0].size() << std::endl;
#endif
            if (polyshapeOut.size() <= 0)
            {
                scale -= xyScale / lastScale;
            }
            else
            {
                if (polyshapeOut.size() == 1 && polyshapeOut[0].size() <= 3)
                    break;
                if (polyshapeOut.size() == 1 && polyshapeOut[0].size() <= 5 && loops > 32)
                    break;
                scale += xyScale / lastScale;
            }
            lastScale = (lastScale << 1) & ScaleMax;
#ifdef DEBUG
            if (debug > 1)
                std::cout << "OffsetPaths: scale:     " << scale << std::endl;
            if (debug > 1)
                std::cout << "OffsetPaths: lastScale: " << lastScale << std::endl;
#endif
        } while (lastScale != 0 || loops > 64);
#ifdef DEBUG
        if (debug > 0)
            std::cout << "MinimumPolygon: finished: loops: " << loops << std::endl;
#endif
        if (polyshapeOut.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshapeOut, doubleType));
        }
    }

    void clip(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        bool doubleType = false;
        ClipType clipType = ctIntersection;
        Local<String> errMsg = checkArguments(args, 1);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args.Length() > 2 && args[2]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        if (args.Length() > 3)
        {
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "ctUnion", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                clipType = ctUnion;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "ctDifference", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                clipType = ctDifference;
            }
            if (args[3]->StrictEquals(String::NewFromUtf8(isolate, "ctXor", v8::NewStringType::kNormal).ToLocalChecked()))
            {
                clipType = ctXor;
            }
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[3]: clipType: " << *v8::String::Utf8Value(isolate, args[3]) << std::endl;
#endif
        }
        Clipper clipper;
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        if (polyshape.size() <= 0)
        {
            return;
        }
        clipper.AddPaths(polyshape, ptSubject, true);
        polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[1]), doubleType);
        if (polyshape.size() <= 0)
        {
            return;
        }
        clipper.AddPaths(polyshape, ptClip, true);
        Paths clipperSolution;
        clipper.Execute(clipType, clipperSolution, pftNonZero, pftNonZero);
#ifdef DEBUG
        if (debug > 1)
            std::cout << "clipperSolution.size(): " << clipperSolution.size() << std::endl;
#endif
        Local<Array> solutions = Array::New(isolate);
        Paths singleSolution;
        unsigned int i = 0;
        while (i < clipperSolution.size())
        {
            do
            {
                singleSolution.push_back(clipperSolution[i]);
                i++;
            } while (i < clipperSolution.size() && !Orientation(clipperSolution[i]));
            solutions->Set(context, solutions->Length(), polygonsToV8Array(isolate, singleSolution, doubleType)).FromJust();
            singleSolution.clear();
        }
#ifdef DEBUG
        if (debug > 1)
            std::cout << "solutions->Length(): " << solutions->Length() << std::endl;
#endif
        if (solutions->Length() > 0)
        {
            args.GetReturnValue().Set(solutions);
        }
    }

    void clean(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        bool doubleType = false;
        double distance = 1.415;
        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        if (args.Length() > 2)
        {
            distance = args[2]->NumberValue(context).FromMaybe(1.415);
#ifdef DEBUG
            if (debug > 0)
                std::cout << "args[2]: distance: " << *v8::String::Utf8Value(isolate, args[2]) << std::endl;
#endif
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        Paths polyshapeOut(polyshape.size());
#ifdef DEBUG
        if (debug > 1)
            std::cout << "before CleanPolygons: polyshape.size(): " << polyshape.size() << std::endl;
#endif
        CleanPolygons(polyshape, polyshapeOut, distance);
#ifdef DEBUG
        if (debug > 1)
            std::cout << "after  CleanPolygons: polyshapeOut.size(): " << polyshapeOut.size() << std::endl;
#endif
        if (polyshapeOut.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshapeOut, doubleType));
        }
    }

    void fixOrientation(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        doFixOrientation(polyshape);
        if (polyshape.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(Isolate::GetCurrent(), polyshape, doubleType));
        }
    }

    void simplify(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->StrictEquals(String::NewFromUtf8(isolate, "double", v8::NewStringType::kNormal).ToLocalChecked()))
        {
            doubleType = true;
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        SimplifyPolygons(polyshape, polyshape, pftNonZero);
        if (polyshape.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(Isolate::GetCurrent(), polyshape, doubleType));
        }
    }

    void Init(Local<Object> exports)
    {
        NODE_SET_METHOD(exports, "setDebug", setDebug);
        NODE_SET_METHOD(exports, "orientation", orientation);
        NODE_SET_METHOD(exports, "offset", offset);
        NODE_SET_METHOD(exports, "minimum", minimum);
        NODE_SET_METHOD(exports, "clip", clip);
        NODE_SET_METHOD(exports, "clean", clean);
        NODE_SET_METHOD(exports, "fixOrientation", fixOrientation);
        NODE_SET_METHOD(exports, "simplify", simplify);
    }

    // Register the module with node. Note that "modulename" must be the same as
    // the basename of the resulting .node file. You can specify that name in
    // binding.gyp ("target_name"). When you change it there, change it here too.
    NODE_MODULE(clipper, Init);

}
