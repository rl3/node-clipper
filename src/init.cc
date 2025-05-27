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
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        if (args.Length() > 0 && args[0]->IsNumber())
        {
            debug = args[0]->NumberValue(context).FromMaybe(0);
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
                point->Set(context, point->Length(), x).ToChecked();
                point->Set(context, point->Length(), y).ToChecked();
                points->Set(context, points->Length(), point).ToChecked();
            }
            result->Set(context, result->Length(), points).ToChecked();
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
            result = String::NewFromUtf8(isolate, "Too few arguments! At least 'polyshape[][][]' and 'pointType' are required!").ToLocalChecked();
            return result;
        }

        if (args.Length() < checkLength)
        {
            result = String::NewFromUtf8(isolate, "Too few arguments!").ToLocalChecked();
            return result;
        }

        if (!args[0]->IsArray())
        {
            result = String::Concat(isolate,
                                    String::NewFromUtf8(isolate, "Wrong argument 'polyshape': array[shapes][points][point] required: ").ToLocalChecked(),
                                    args[0]->ToString(context).ToLocalChecked());
        }

        if (checkLength < 2)
        {
            return result;
        }

        if (!args[1]->IsString() ||
            !(args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false) ||
              args[1]->Equals(context, String::NewFromUtf8(isolate, "integer").ToLocalChecked()).FromMaybe(false)))
        {
            result = String::Concat(isolate,
                                    String::NewFromUtf8(isolate, "Wrong argument 'pointType': 'double' || 'integer' required: ").ToLocalChecked(),
                                    args[1]->ToString(context).ToLocalChecked());
            return result;
        }

        if ((args.Length() > 2) && (checkLength > 2))
        {
            if (!args[2]->IsNumber())
            {
                result = String::Concat(isolate,
                                        String::NewFromUtf8(isolate, "Wrong argument 'delta' || 'distance': number required: ").ToLocalChecked(),
                                        args[2]->ToString(context).ToLocalChecked());
                return result;
            }
        }

        if ((args.Length() > 3) && (checkLength > 3))
        {
            if (!args[3]->IsString() ||
                !(args[3]->Equals(context, String::NewFromUtf8(isolate, "jtMiter").ToLocalChecked()).FromMaybe(false) ||
                  args[3]->Equals(context, String::NewFromUtf8(isolate, "jtSquare").ToLocalChecked()).FromMaybe(false) ||
                  args[3]->Equals(context, String::NewFromUtf8(isolate, "jtRound").ToLocalChecked()).FromMaybe(false)))
            {
                result = String::Concat(isolate,
                                        String::NewFromUtf8(isolate, "Wrong argument 'joinType': 'jtMiter' || 'jtSquare' || 'jtRound' required: ").ToLocalChecked(),
                                        args[3]->ToString(context).ToLocalChecked());
                return result;
            }
        }

        if ((args.Length() > 4) && (checkLength > 4))
        {
            if (!args[4]->IsNumber())
            {
                result = String::Concat(isolate,
                                        String::NewFromUtf8(isolate, "Wrong argument 'miterLimit': number required: ").ToLocalChecked(),
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
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
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
            orientations->Set(context, orientations->Length(), Boolean::New(isolate, polyOrientation)).ToChecked();
        }
        args.GetReturnValue().Set(orientations);
    }

    void offset(const FunctionCallbackInfo<Value> &args)
    {
        Isolate *isolate = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();

        JoinType joinType = jtMiter;
        EndType_ endType = etClosed;
        double miterLimit = 30.0;
        long delta;
        bool doubleType = false;
        Local<String> errMsg = checkArguments(args, 3);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
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
        if (args.Length() > 3)
        {
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtMiter").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtMiter;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtSquare").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtSquare;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtRound").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtRound;
            }
        }
        if (args.Length() > 4)
        {
            miterLimit = args[4]->NumberValue(context).FromMaybe(0);
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        doFixOrientation(polyshape);
        Paths polyshapeOut;
        OffsetPaths(polyshape, polyshapeOut, delta, joinType, endType, miterLimit);
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
        EndType_ endType = etClosed;
        double miterLimit = 30.0;
        bool doubleType = false;

        Local<String> errMsg = checkArguments(args, 2);
        if (errMsg->Length() > 0)
        {
            isolate->ThrowException(Exception::TypeError(errMsg));
            return;
        }
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
        {
            doubleType = true;
        }
        if (args.Length() > 3)
        {
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtMiter").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtMiter;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtSquare").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtSquare;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "jtRound").ToLocalChecked()).FromMaybe(false))
            {
                joinType = jtRound;
            }
        }
        if (args.Length() > 4)
        {
            miterLimit = args[4]->NumberValue(context).FromMaybe(0);
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
        xScale = signedSum(xMin, xMax) / 2;
        yScale = signedSum(yMin, yMax) / 2;
        xyScale = std::min(std::abs(xScale), std::abs(yScale));

        int s = -1;
        int loops = 0;
        long scale = xyScale;
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
                std::cout << "node-clipper: exception from ClipperLib caught! miterLimit reduced: " << miterLimit << std::endl;
                continue;
            }
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
        } while (lastScale != 0 || loops > 64);

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
        if (args[2]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
        {
            doubleType = true;
        }
        if (args.Length() > 3)
        {
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "ctUnion").ToLocalChecked()).FromMaybe(false))
            {
                clipType = ctUnion;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "ctDifference").ToLocalChecked()).FromMaybe(false))
            {
                clipType = ctDifference;
            }
            if (args[3]->Equals(context, String::NewFromUtf8(isolate, "ctXor").ToLocalChecked()).FromMaybe(false))
            {
                clipType = ctXor;
            }
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
            solutions->Set(context, solutions->Length(), polygonsToV8Array(isolate, singleSolution, doubleType)).ToChecked();
            singleSolution.clear();
        }
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
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
        {
            doubleType = true;
        }
        if (args.Length() > 2)
        {
            distance = args[2]->NumberValue(context).FromMaybe(1.415);
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        Paths polyshapeOut(polyshape.size());
        CleanPolygons(polyshape, polyshapeOut, distance);
        if (polyshapeOut.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshape, doubleType));
        }
    }

    void fixOrientation(const FunctionCallbackInfo<Value> &args)
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
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
        {
            doubleType = true;
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        doFixOrientation(polyshape);
        if (polyshape.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshape, doubleType));
        }
    }

    void simplify(const FunctionCallbackInfo<Value> &args)
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
        if (args[1]->Equals(context, String::NewFromUtf8(isolate, "double").ToLocalChecked()).FromMaybe(false))
        {
            doubleType = true;
        }
        Paths polyshape = v8ArrayToPolygons(Local<Array>::Cast(args[0]), doubleType);
        SimplifyPolygons(polyshape, polyshape, pftNonZero);
        if (polyshape.size() > 0)
        {
            args.GetReturnValue().Set(polygonsToV8Array(isolate, polyshape, doubleType));
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

    // Register the module with node.
    NODE_MODULE(clipper, Init);

} // namespace demo
